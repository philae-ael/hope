#include "mesh.h"
#include "core/core/memory.h"

#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/sync.h>
#include <engine/graphics/vulkan/vulkan.h>

#include <core/containers/vec.h>
#include <core/core.h>
#include <core/core/sched.h>
#include <core/fs/fs.h>

#include <cgltf.h>
#include <stb_image.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

void unload_mesh(subsystem::video& v, GpuMesh mesh) {
  vmaDestroyBuffer(v.device.allocator, mesh.vertex_buffer, mesh.vertex_buf_allocation);
  vmaDestroyBuffer(v.device.allocator, mesh.index_buffer, mesh.index_buf_allocation);
  mesh.base_color.destroy(v.device);
}

struct LoadMeshTask {
  core::Arena* arena;

  core::Task* task;
  vk::Device& device;
  cgltf_data* data;
  MeshToken mesh_token;
  MeshLoader* mesh_loader;

  usize stack[16];
  usize stack_depth = 0;

  core::TaskReturn operator()(core::TaskQueue*) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocate_info{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = mesh_loader->pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    vkAllocateCommandBuffers(device, &allocate_info, &cmd);

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(cmd, &begin_info);
    defer {
      vkEndCommandBuffer(cmd);
      VkCommandBufferSubmitInfo command_buffer_info{
          .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
          .commandBuffer = cmd,
      };
      VkSubmitInfo2 submit{
          .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
          .commandBufferInfoCount = 1,
          .pCommandBufferInfos    = &command_buffer_info,
      };
      vkQueueSubmit2(device.omni_queue, 1, &submit, VK_NULL_HANDLE);
    };

    LOG_TRACE("UPLOAD WORKER %zu", stack_depth);
    auto& scene = *data->scene;

    if (stack_depth == 0) {
      stack_depth++;
      stack[0] = 0;
    }

    upload_node(cmd, scene.nodes[stack[0]], 1);

    if (stack_depth == 1) {
      stack[0] += 1;
      if (scene.nodes_count >= stack[0]) {
        stack_depth--;
      }
    }

    if (stack_depth == 0) {
      stack_depth--;
      mesh_loader->mesh_job_infos[mesh_token].expect("mesh info does not exist?!").staging_done = true;
      return core::TaskReturn::Stop;
    } else {
      return core::TaskReturn::Yield;
    }
  }

  void upload_node(VkCommandBuffer cmd, cgltf_node* node, usize depth) {
    if (depth == stack_depth) {
      stack_depth++;
      stack[depth] = 0;

      math::Mat4 transform = math::Mat4::Id;
      cgltf_node_transform_world(node, transform._coeffs);
      LOG2_TRACE("node: ", node->name);
      if (node->mesh != nullptr) {
        upload_mesh(cmd, *node->mesh, transform);
      }
    }

    if (node->children_count > stack[depth]) {
      upload_node(cmd, node->children[stack[depth]], depth + 1);
    }

    if (stack_depth == depth + 1) {
      stack[depth]++;
      if (node->children_count <= stack[depth]) {
        stack_depth--;
      }
    }
  }

  core::TaskReturn upload_mesh(VkCommandBuffer cmd, cgltf_mesh& mesh, const math::Mat4& transform) {
    const usize TEXEL_SIZE      = 4 * sizeof(u8);
    const VkFormat TEXEL_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

    // --- ESTIMATE MEMORY NEEDED ---
    usize staging_buffer_size{};
    for (auto& primitive : core::storage{mesh.primitives_count, mesh.primitives}.iter()) {
      // === Index buffer ===

      switch (primitive.indices->component_type) {
      case cgltf_component_type_r_16u:
        staging_buffer_size += sizeof(u16) * primitive.indices->count;
        break;
      case cgltf_component_type_r_32u:
        staging_buffer_size += sizeof(u32) * primitive.indices->count;
        break;
      default:
        ASSERTM(false, "Component type not supported");
      }

      staging_buffer_size += primitive.attributes[0].data->count * (4 + 4 + 2) * sizeof(f32);

      staging_buffer_size = ALIGN_UP(staging_buffer_size, TEXEL_SIZE);

      // === Materials ===
      if (primitive.material != nullptr) {
        auto& material = *primitive.material;
        if (material.has_pbr_metallic_roughness &&
            material.pbr_metallic_roughness.base_color_texture.texture != nullptr) {

          auto buf_view = material.pbr_metallic_roughness.base_color_texture.texture->image->buffer_view;
          int x, y, comp;
          stbi_info_from_memory(
              (const stbi_uc*)buf_view->buffer->data + buf_view->offset, (int)buf_view->size, &x, &y, &comp
          );

          auto texture_index = INDEX_OF(material.pbr_metallic_roughness.base_color_texture.texture, data->textures);
          ASSERT(texture_index >= 0);
          ASSERT(usize(texture_index) < data->textures_count);

          staging_buffer_size += TEXEL_SIZE * usize(x) * usize(y);
        }
      }
    }

    StagingBufferToken staging_token;
    {
      core::Maybe<StagingBuffer> staging_ = StagingBuffer::init(device, usize(4 * staging_buffer_size), cmd);
      if (staging_.is_none()) {
        return core::TaskReturn::Yield;
      }

      staging_token = mesh_loader->staging_buffers.insert(
          core::get_named_allocator(core::AllocatorName::General), std::move(staging_.value())
      );
    }

    StagingBuffer& staging = mesh_loader->staging_buffers.get(staging_token).value();

    for (auto& primitive : core::storage{mesh.primitives_count, mesh.primitives}.iter()) {
      ASSERT(primitive.type == cgltf_primitive_type_triangles);

      auto tmp_arena            = arena->make_temp();
      core::Allocator tmp_alloc = tmp_arena;

      GpuMesh gpu_mesh{.transform = transform};

      // === Index buffer ===

      core::LayoutInfo component_layout;
      switch (primitive.indices->component_type) {
      case cgltf_component_type_r_16u:
        component_layout = core::default_layout_of<u16>();
        break;
      case cgltf_component_type_r_32u:
        component_layout = core::default_layout_of<u32>();
        break;
      default:
        ASSERTM(false, "Component type not supported");
      }
      {
        auto indices = tmp_alloc.allocate_array(component_layout, primitive.indices->count);

        ASSERT(cgltf_accessor_unpack_indices(primitive.indices, indices.data, component_layout.size, indices.size));
        VkBufferCreateInfo index_buf_create_info{
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size                  = indices.into_bytes().size,
            .usage                 = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices   = &device.omni_queue_family_index
        };
        VmaAllocationCreateInfo alloc_create_info{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        vmaCreateBuffer(
            device.allocator, &index_buf_create_info, &alloc_create_info, &gpu_mesh.index_buffer,
            &gpu_mesh.index_buf_allocation, nullptr
        );
        vmaCopyMemoryToAllocation(
            device.allocator, indices.data, gpu_mesh.index_buf_allocation, 0, indices.into_bytes().size
        );
        gpu_mesh.indice_count = (u32)indices.size / component_layout.size;
        gpu_mesh.huge_indices = primitive.indices->component_type == cgltf_component_type_r_32u;
      }

      // === Vertex buffer ===
      // pos: 4
      // normal: 4
      // texcoord: 2

      {
        usize vertex_size = primitive.attributes[0].data->count * (4 + 4 + 2) * sizeof(f32);

        VkBufferCreateInfo vertex_buf_create_info{
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size                  = vertex_size,
            .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices   = &device.omni_queue_family_index
        };
        VmaAllocationCreateInfo alloc_create_info{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        vmaCreateBuffer(
            device.allocator, &vertex_buf_create_info, &alloc_create_info, &gpu_mesh.vertex_buffer,
            &gpu_mesh.vertex_buf_allocation, nullptr
        );

        void* dst_data = nullptr;
        VK_ASSERT(vmaMapMemory(device.allocator, gpu_mesh.vertex_buf_allocation, &dst_data));

        auto vertices = core::storage<f32>{vertex_size / sizeof(f32), (f32*)dst_data};
        usize offset  = 0;
        for (auto i : core::range{0zu, primitive.attributes[0].data->count}.iter()) {
          for (auto& attribute : core::storage{primitive.attributes_count, primitive.attributes}.iter()) {
            ASSERT(primitive.attributes[0].data->count == attribute.data->count);

            switch (attribute.type) {
            case cgltf_attribute_type_position:
              ASSERT(cgltf_accessor_read_float(attribute.data, i, vertices.data + offset + 0, 3));
              break;
            case cgltf_attribute_type_normal:
              ASSERT(cgltf_accessor_read_float(attribute.data, i, vertices.data + offset + 3, 3));
              break;
            case cgltf_attribute_type_texcoord:
              ASSERT(cgltf_accessor_read_float(attribute.data, i, vertices.data + offset + 6, 2));
              break;
            case cgltf_attribute_type_tangent:
              LOG_WARNING("tangent attribute type not supported");
              break;
            case cgltf_attribute_type_color:
              LOG_WARNING("color attribute type not supported");
              break;
            default:
              LOG_WARNING("<unknown> attribute type not supported");
              break;
            }
          }
          offset += 3 + 3 + 2;
        }

        {
          vmaUnmapMemory(device.allocator, gpu_mesh.vertex_buf_allocation);
          usize local_offset = 0;
          VK_ASSERT(
              vmaFlushAllocations(device.allocator, 1, &gpu_mesh.vertex_buf_allocation, &local_offset, &vertex_size)
          );
        }
      }

      // === Materials ===
      if (primitive.material != nullptr) {
        auto& material = *primitive.material;
        if (material.has_pbr_metallic_roughness &&
            material.pbr_metallic_roughness.base_color_texture.texture != nullptr) {

          auto buf_view = material.pbr_metallic_roughness.base_color_texture.texture->image->buffer_view;
          int x, y, channels;

          // This is the slow part of loading a mesh so maybe... do  it on another thread
          LOG2_TRACE("decode image start");
          u8* mem = stbi_load_from_memory(
              (const stbi_uc*)buf_view->buffer->data + buf_view->offset, (int)buf_view->size, &x, &y, &channels, 4
          );
          if (staging.image_enough_memory(TEXEL_SIZE, (u32)x, (u32)y)) {
            ASSERTM(mem != 0, "can't load texture: %s", stbi_failure_reason());

            vk::image2D::ConfigExtentValues config_extent_values{};
            gpu_mesh.base_color = vk::image2D::create(
                device, config_extent_values,
                vk::image2D::Config{
                    .format = TEXEL_FORMAT,
                    .extent = {.constant{.width = (u32)x, .height = (u32)y}},
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                },
                {}
            );

            vk::pipeline_barrier(cmd, gpu_mesh.base_color.sync_to({VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}));
            staging.cmdCopyMemoryToImage(device, cmd, mem, gpu_mesh.base_color, sizeof(u8) * 4, {}, (u32)x, (u32)y);
          } else {
            LOG2_TRACE("staging full");
          }
          stbi_image_free(mem);
        }
      }

      mesh_loader->mesh_job_infos[mesh_token].expect("mesh info does not exist?!").inflight += 1;
      mesh_loader->jobs.push(
          core::get_named_allocator(core::AllocatorName::General),
          Job{
              mesh_token,
              staging_token,
              gpu_mesh,
          }
      );
    }
    staging.cmdClose(cmd);
    return core::TaskReturn::Yield;
  }
  ~LoadMeshTask() {
    cgltf_free(data);
    core::arena_dealloc(*arena); // ? should Probably be done externally by mesh loader
  }
};

MeshToken MeshLoader::queue_mesh(vk::Device& device, core::str8 src) {
  LOG2_INFO("loading mesh from ", src);
  auto alloc = core::get_named_allocator(core::AllocatorName::General);

  auto& arena                 = core::arena_alloc();
  core::Allocator arena_alloc = arena;

  MeshToken mesh_token  = mesh_job_infos.insert(alloc, {});
  const char* path      = fs::resolve_path(arena_alloc, src).expect("can't find mesh").cstring(arena_alloc);
  cgltf_options options = {
      .type   = cgltf_file_type_glb,
      .memory = {
          .alloc_func =
              [](void* arena, usize size) {
                core::Allocator alloc = *(core::Arena*)arena;
                return alloc.allocate(size);
              },
          .free_func =
              [](void* arena, void* ptr) {
                core::Allocator alloc = *(core::Arena*)arena;
                return alloc.deallocate(ptr, 0);
              },
          .user_data = &arena,
      },
  };

  LOG2_TRACE("loading into memory ", src);
  cgltf_data* data    = NULL;
  cgltf_result result = cgltf_parse_file(&options, path, &data);
  if (result != cgltf_result_success) {
    core::panic("can't load gltf: %u", result);
  }
  result = cgltf_load_buffers(&options, data, path);
  if (result != cgltf_result_success) {
    core::panic("can't load gltf: %u", result);
  }

  auto* task = core::default_task_queue()->allocate_job();
  *task      = core::Task::from(
      [](LoadMeshTask* async_mesh_loader, core::TaskQueue* q) { return (*async_mesh_loader)(q); },
      new (arena_alloc.allocate<LoadMeshTask>()) LoadMeshTask{
          &arena,
          task,
          device,
          data,
          mesh_token,
          this,
      }
  );

  LOG2_INFO("loading of mesh ", src, " has been launched");
  return mesh_token;
}
