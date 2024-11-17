#include "mesh.h"
#include "app/renderer.h"
#include "core/core/memory.h"
#include "engine/utils/time.h"

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
}

// TODO: transform this big coroutine like task into a set of smaller, independant tasks
struct LoadMeshTask {
  core::Arena* arena;

  vk::Device& device;
  cgltf_data* data;
  MeshToken mesh_token;
  MeshLoader* mesh_loader;
  TextureCache* tex_cache;

  usize stack[16];
  usize stack_depth = 0;

  core::TaskReturn operator()(core::TaskQueue*) {
    auto s = utils::scope_start("Load Mesh Task"_hs);
    defer { utils::scope_end(s); };

    auto cmdtok = mesh_loader->command_buffers.insert(
        core::get_named_allocator(core::AllocatorName::General), CommandBuffer::init(device, mesh_loader->pool)
    );

    CommandBuffer command_buffer = mesh_loader->command_buffers.get(cmdtok).expect("idk why");
    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(command_buffer.cmd, &begin_info);

    defer {
      vkEndCommandBuffer(command_buffer.cmd);
      command_buffer.submit(device.omni_queue);
    };

    auto& scene = *data->scene;

    if (stack_depth == 0) {
      stack_depth++;
      stack[0] = 0;
    }

    upload_node(command_buffer.cmd, cmdtok, scene.nodes[stack[0]], 1);

    if (stack_depth == 1) {
      stack[0] += 1;
      if (scene.nodes_count >= stack[0]) {
        stack_depth--;
      }
    }

    if (stack_depth == 0) {
      stack_depth--;
      mesh_loader->mesh_job_infos[mesh_token].expect("mesh info does not exist?!").staging_done = true;
      LOG_INFO("mesh done!");
      return core::TaskReturn::Stop;
    } else {
      return core::TaskReturn::Yield;
    }
  }

  void upload_node(VkCommandBuffer cmd, CommandBufferToken cmdtok, cgltf_node* node, usize depth) {
    if (depth == stack_depth) {
      stack_depth++;
      stack[depth] = 0;

      math::Mat4 transform = math::Mat4::Id;
      cgltf_node_transform_world(node, transform._coeffs);
      if (node->mesh != nullptr) {
        upload_mesh(cmd, cmdtok, *node->mesh, transform);
      }
    }

    if (node->children_count > stack[depth]) {
      upload_node(cmd, cmdtok, node->children[stack[depth]], depth + 1);
    }

    if (stack_depth == depth + 1) {
      stack[depth]++;
      if (node->children_count <= stack[depth]) {
        stack_depth--;
      }
    }
  }

  core::TaskReturn upload_mesh(
      VkCommandBuffer cmd,
      CommandBufferToken cmdtok,
      cgltf_mesh& mesh,
      const math::Mat4& transform
  ) {
    const usize TEXEL_SIZE      = 4 * sizeof(u8);
    const VkFormat TEXEL_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

    // --- ESTIMATE MEMORY NEEDED ---
    usize staging_buffer_size{};
    for (auto& primitive : core::storage{mesh.primitives_count, mesh.primitives}.iter()) {
      staging_buffer_size = ALIGN_UP(staging_buffer_size, TEXEL_SIZE);

      // === Materials ===
      if (primitive.material != nullptr) {
        auto& material = *primitive.material;
        if (material.has_pbr_metallic_roughness &&
            material.pbr_metallic_roughness.base_color_texture.texture != nullptr) {

          auto image_index = cgltf_image_index(data, material.pbr_metallic_roughness.base_color_texture.texture->image);
          if (tex_cache->entry({.src = "GLTF"_s, .texture_index = usize(image_index)}).is_empty()) {

            auto buf_view = data->images[image_index].buffer_view;
            int x, y, comp;
            stbi_info_from_memory(
                (const stbi_uc*)buf_view->buffer->data + buf_view->offset, (int)buf_view->size, &x, &y, &comp
            );

            staging_buffer_size += TEXEL_SIZE * usize(x) * usize(y);
          }
        }
      }
    }

    // Note that if size == 0, there is no need for a command buffer Maybe do somthing about that

    core::Maybe<StagingBuffer> staging_;
    if (staging_buffer_size > 0) {
      for (auto [idx, buffer] :
           core::enumerate_rev{mesh_loader->staging_buffers.iter_rev(), mesh_loader->staging_buffers.size() - 1}) {
        if (buffer->size >= staging_buffer_size) {
          buffer->reset();
          staging_ = *buffer;

          mesh_loader->staging_buffers.swap_last_pop(idx);
        }
      }
    }
    if (staging_.is_none()) {
      staging_ = StagingBuffer::init(device, staging_buffer_size > 0 ? MAX(staging_buffer_size, MB(64)) : 0, cmd);
    }
    if (staging_.is_none()) {
      LOG_WARNING("can't get a staging buffer, probably not enough memory");
      return core::TaskReturn::Yield;
    }

    auto staging_buffer_token = mesh_loader->inflight_staging_buffers.insert(
        core::get_named_allocator(core::AllocatorName::General), {0, staging_.value()}
    );
    RefCountedStagingBuffer& staging = mesh_loader->inflight_staging_buffers[staging_buffer_token].value();

    for (auto& primitive : core::storage{mesh.primitives_count, mesh.primitives}.iter()) {
      ASSERT(primitive.type == cgltf_primitive_type_triangles);

      auto tmp_arena            = arena->make_temp();
      core::Allocator tmp_alloc = tmp_arena;

      GpuMesh gpu_mesh{.transform = transform};
      {
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
            .flags         = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage         = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
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

      {
        // === Vertex buffer ===
        usize vertex_size = primitive.attributes[0].data->count * sizeof(Vertex);

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

        auto vertices = core::storage<Vertex>{primitive.attributes[0].data->count, (Vertex*)dst_data};
        for (auto i : core::range{0zu, primitive.attributes[0].data->count}.iter()) {
          for (auto& attribute : core::storage{primitive.attributes_count, primitive.attributes}.iter()) {
            ASSERT(primitive.attributes[0].data->count == attribute.data->count);

            switch (attribute.type) {
            case cgltf_attribute_type_position:
              ASSERT(cgltf_accessor_read_float(attribute.data, i, &vertices[i].x, 3));
              break;
            case cgltf_attribute_type_normal:
              ASSERT(cgltf_accessor_read_float(attribute.data, i, &vertices[i].nx, 3));
              break;
            case cgltf_attribute_type_texcoord:
              if (attribute.index == 0) {
                ASSERT(cgltf_accessor_read_float(attribute.data, i, &vertices[i].u, 2));
              }
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

          auto image_index = cgltf_image_index(data, material.pbr_metallic_roughness.base_color_texture.texture->image);

          gpu_mesh.base_color_texture_idx =
              tex_cache->entry({.src = "GLTF"_s, /* TODO: use path? */ .texture_index = usize(image_index)})
                  .or_create([&]() {
                    auto buf_view = data->images[image_index].buffer_view;
                    int x, y, channels;

                    /// This seems to be the slowest part now
                    u8* mem = stbi_load_from_memory(
                        (const stbi_uc*)buf_view->buffer->data + buf_view->offset, (int)buf_view->size, &x, &y,
                        &channels, 4
                    );

                    ASSERTM(mem != 0, "can't load texture: %s", stbi_failure_reason());

                    LOG_INFO("decode image index %zu of size %dX%d", image_index, x, y);
                    vk::image2D::ConfigExtentValues config_extent_values{};
                    auto image = vk::image2D::create(
                        device, config_extent_values,
                        vk::image2D::Config{
                            .format            = TEXEL_FORMAT,
                            .extent            = {.constant{.width = (u32)x, .height = (u32)y}},
                            .tiling            = VK_IMAGE_TILING_OPTIMAL,
                            .usage             = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            .alloc_create_info = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE},
                        },
                        {}
                    );

                    vk::pipeline_barrier(cmd, image.sync_to({VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}));
                    staging.buffer.cmdCopyMemoryToImage(device, cmd, mem, image, TEXEL_SIZE, {}, (u32)x, (u32)y);
                    vk::pipeline_barrier(cmd, image.sync_to({VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}));
                    stbi_image_free(mem);

                    return image;
                  });
        }
      }

      mesh_loader->mesh_job_infos[mesh_token].expect("mesh info does not exist?!").inflight += 1;
      staging.inflight++;
      mesh_loader->jobs.push(
          core::get_named_allocator(core::AllocatorName::General),
          Job{
              mesh_token,
              staging_buffer_token,
              gpu_mesh,
              cmdtok,
          }
      );
    }

    return core::TaskReturn::Yield;
  }

  ~LoadMeshTask() {
    cgltf_free(data);
    core::arena_dealloc(*arena);
  }
};

MeshToken MeshLoader::queue_mesh(vk::Device& device, core::str8 src, TextureCache& texture_cache) {
  LOG2_INFO("loading mesh from ", src);
  auto alloc = core::get_named_allocator(core::AllocatorName::General);

  auto& arena                 = core::arena_alloc();
  core::Allocator arena_alloc = arena;

  MeshToken mesh_token  = mesh_job_infos.insert(alloc, {});
  MeshJobInfo& mesh_job = mesh_job_infos.get(mesh_token).expect("?");

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

  mesh_job.task  = core::default_task_queue()->allocate_job();
  *mesh_job.task = core::Task::from(
      [](LoadMeshTask* async_mesh_loader, core::TaskQueue* q) { return (*async_mesh_loader)(q); },
      new (arena_alloc.allocate<LoadMeshTask>()) LoadMeshTask{
          &arena,
          device,
          data,
          mesh_token,
          this,
          &texture_cache,
      }
  );

  LOG2_INFO("loading of mesh ", src, " has been launched");
  return mesh_token;
}

void MeshLoader::work(vk::Device& device, OnPrimitiveLoaded callback, void* userdata) {
  for (auto t : command_buffers.iter_rev_enumerate()) {
    CommandBufferToken command_buffer_token = *core::get<0>(t);
    CommandBuffer& buffer                   = *core::get<1>(t);

    if (!buffer.done(device)) {
      continue;
    }

    for (auto [idx, job] : core::enumerate_rev{jobs.iter_rev(), jobs.size() - 1}) {
      if (job->command_buffer_token == command_buffer_token) {
        // check if mesh is done after this job
        auto& infos = mesh_job_infos[job->mesh_token].expect("counter does not exist!");
        infos.inflight--;

        bool mesh_fully_loaded = infos.staging_done && infos.inflight == 0;

        callback(userdata, device, job->mesh_token, job->mesh, mesh_fully_loaded);

        if (mesh_fully_loaded) {
          auto& load_task = *static_cast<LoadMeshTask*>(infos.task->data);
          load_task.~LoadMeshTask();
          core::default_task_queue()->deallocate_job(infos.task);
          mesh_job_infos.destroy(job->mesh_token);
        }

        auto job             = jobs.swap_last_pop(idx);
        auto& staging_buffer = inflight_staging_buffers.get(job.staging_buffer_token).expect("huhu");
        staging_buffer.inflight--;
        if (staging_buffer.inflight == 0) {
          if (staging_buffer.buffer.size > 0 && staging_buffers.size() < staging_buffers.capacity()) {
            staging_buffers.push(core::noalloc, staging_buffer.buffer);
          } else {
            staging_buffer.buffer.uninit(device);
          }
          inflight_staging_buffers.destroy(job.staging_buffer_token);
        };
      }
    }

    buffer.uninit(device, pool);
    command_buffers.destroy(command_buffer_token);
  }
}

MeshLoader::MeshLoader(vk::Device& device) {
  VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
  };
  VK_ASSERT(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &pool));
}

void MeshLoader::uninit(subsystem::video& v) {
  vkDeviceWaitIdle(v.device);

  for (auto& cmd : command_buffers.iter()) {
    cmd.uninit(v.device, pool);
  }
  command_buffers.reset(core::get_named_allocator(core::AllocatorName::General));

  for (auto& job : jobs.iter()) {
    unload_mesh(v, job.mesh);
  }
  jobs.reset(core::get_named_allocator(core::AllocatorName::General));

  for (auto& inflight_staging_buffer : inflight_staging_buffers.iter()) {
    inflight_staging_buffer.buffer.uninit(v.device);
  }
  inflight_staging_buffers.reset(core::get_named_allocator(core::AllocatorName::General));

  for (auto& staging_buffer : staging_buffers.iter()) {
    staging_buffer.uninit(v.device);
  }
  staging_buffers.reset(core::noalloc);

  for (auto mesh_job_info : mesh_job_infos.iter()) {
    auto& load_task = *static_cast<LoadMeshTask*>(mesh_job_info.task->data);
    load_task.~LoadMeshTask();
    core::default_task_queue()->deallocate_job(mesh_job_info.task);
  }
  mesh_job_infos.reset(core::get_named_allocator(core::AllocatorName::General));

  vkDestroyCommandPool(v.device, pool, nullptr);
}
