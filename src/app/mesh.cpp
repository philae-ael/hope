#include "mesh.h"
#include "engine/graphics/vulkan/sync.h"

#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/vulkan.h>

#include <core/containers/vec.h>
#include <core/core.h>
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

MeshToken MeshLoader::queue_mesh(vk::Device& device, VkCommandBuffer cmd, core::str8 src) {
  LOG2_INFO("loading mesh from ", src);
  auto alloc = core::get_named_allocator(core::AllocatorName::General);

  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator scratch_alloc = scratch;

  MeshToken mesh_token = mesh_job_infos.insert(alloc, {});
  auto& infos          = mesh_job_infos[mesh_token].value();

  const char* path      = fs::resolve_path(*scratch, src).expect("can't find mesh").cstring(scratch);
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
          .user_data = scratch.arena_,
      },
  };

  LOG2_TRACE("loading into memory ", src);
  cgltf_data* data    = NULL;
  cgltf_result result = cgltf_parse_file(&options, path, &data);
  if (result != cgltf_result_success) {
    core::panic("can't load gltf: %u", result);
  }
  defer { cgltf_free(data); };
  result = cgltf_load_buffers(&options, data, path);
  if (result != cgltf_result_success) {
    core::panic("can't load gltf: %u", result);
  }

  LOG2_TRACE("loaded into memory ", src);

  usize staging_buffer_size = 0;
  for (const auto& buffer : core::storage{data->buffers_count, data->buffers}.iter()) {
    staging_buffer_size += buffer.size;
  }
  for (const auto& texture : core::storage{data->buffers_count, data->buffers}.iter()) {
    staging_buffer_size += texture.size;
  }
  auto staging_token =
      staging_buffers.insert(alloc, StagingBuffer::init(device, usize(1.5 * (f32)staging_buffer_size)));
  auto& staging = staging_buffers[staging_token].value();

  auto work_on_mesh = [&](cgltf_mesh& mesh, const math::Mat4& transform) {
    for (auto& primitive : core::storage{mesh.primitives_count, mesh.primitives}.iter()) {
      ASSERT(primitive.type == cgltf_primitive_type_triangles);

      auto tmpArena = scratch->make_temp();
      defer { tmpArena.retire(); };

      GpuMesh gpu_mesh{.transform = transform};

      // === Index buffer ===

      ASSERT(primitive.indices->component_type == cgltf_component_type_r_16u);
      {
        auto indices = scratch_alloc.allocate_array<u16>(primitive.indices->count);

        ASSERT(cgltf_accessor_unpack_indices(primitive.indices, indices.data, 2, indices.size));
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
        staging.copyMemoryToBuffer(device, cmd, indices.data, gpu_mesh.index_buffer, 0, indices.into_bytes().size);
        gpu_mesh.indice_count = (u32)indices.size;
      }

      // === Vertex buffer ===
      // pos: 3
      // normal: 3
      // texcoord: 2

      auto vertices = scratch_alloc.allocate_array<f32>(primitive.attributes[0].data->count * (4 + 4 + 2));

      usize offset = 0;
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
            ASSERT(attribute.index == 0);
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

      VkBufferCreateInfo vertex_buf_create_info{
          .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size                  = vertices.into_bytes().size,
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

      staging.copyMemoryToBuffer(device, cmd, vertices.data, gpu_mesh.vertex_buffer, 0, vertices.into_bytes().size);

      // === Materials ===
      if (primitive.material != nullptr) {
        auto& material = *primitive.material;
        ASSERT(material.has_pbr_metallic_roughness);

        auto buf_view = material.pbr_metallic_roughness.base_color_texture.texture->image->buffer_view;
        int x, y, channels;

        // This is the slow part of loading a mesh so maybe... do  it on another thread
        // LOG2_TRACE("decode image start");
        u8* mem = stbi_load_from_memory(
            (const stbi_uc*)buf_view->buffer->data + buf_view->offset, (int)buf_view->size, &x, &y, &channels, 4
        );
        // LOG2_TRACE("decode image end");
        ASSERTM(mem != 0, "can't load texture: %s", stbi_failure_reason());

        vk::image2D::ConfigExtentValues config_extent_values{};
        gpu_mesh.base_color = vk::image2D::create(
            device, config_extent_values,
            vk::image2D::Config{
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .extent = {.constant{.width = (u32)x, .height = (u32)y}},
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            },
            {}
        );

        vk::pipeline_barrier(cmd, gpu_mesh.base_color.sync_to({VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}));
        staging.copyMemoryToImage(device, cmd, mem, gpu_mesh.base_color, sizeof(u8) * 4, {}, (u32)x, (u32)y);
        stbi_image_free(mem);
      }

      infos.counter += 1;
      jobs.push(
          alloc,
          {
              mesh_token,
              staging_token,
              gpu_mesh,
          }
      );
    }
  };

  auto work_on_node = [&](cgltf_node& node) {
    for (auto& node : core::storage{data->scene->nodes_count, data->scene->nodes}.iter()) {
      LOG2_TRACE("node start");
      if (node->mesh != nullptr) {
        math::Mat4 transform = math::Mat4::Id;
        cgltf_node_transform_world(node, transform._coeffs);

        work_on_mesh(*node->mesh, transform);
      }
      LOG2_TRACE("node end");
    }
  };

  ASSERT(data->scene != nullptr);
  for (auto* node : core::storage{data->scene->nodes_count, data->scene->nodes}.iter()) {
    work_on_node(*node);
  }
  staging.close(cmd);

  LOG2_INFO("loading of mesh ", src, " is done (still waiting data to copied onto the gpu)");
  return mesh_token;
}
