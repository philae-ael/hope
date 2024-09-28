#include "mesh.h"

#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/vulkan.h>

#include <core/containers/vec.h>
#include <core/core.h>
#include <core/fs/fs.h>

#include <cgltf.h>
#include <stb_image.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

core::vec<GpuMesh> load_mesh(core::Allocator alloc, subsystem::video& v, core::str8 src) {
  core::vec<GpuMesh> meshes{};

  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator scratch_alloc = scratch;

  const char* path      = fs::resolve_path(*scratch, src).cstring(scratch_alloc);
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
            .usage                 = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices   = &v.device.omni_queue_family_index
        };
        VmaAllocationCreateInfo alloc_create_info{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        vmaCreateBuffer(
            v.allocator, &index_buf_create_info, &alloc_create_info, &gpu_mesh.index_buffer,
            &gpu_mesh.index_buf_allocation, nullptr
        );
        vmaCopyMemoryToAllocation(
            v.allocator, indices.data, gpu_mesh.index_buf_allocation, 0, indices.into_bytes().size
        );
        gpu_mesh.indices = (u32)indices.size;
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
          .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
          .queueFamilyIndexCount = 1,
          .pQueueFamilyIndices   = &v.device.omni_queue_family_index
      };
      VmaAllocationCreateInfo alloc_create_info{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO,
      };
      vmaCreateBuffer(
          v.allocator, &vertex_buf_create_info, &alloc_create_info, &gpu_mesh.vertex_buffer,
          &gpu_mesh.vertex_buf_allocation, nullptr
      );
      vmaCopyMemoryToAllocation(
          v.allocator, vertices.data, gpu_mesh.vertex_buf_allocation, 0, vertices.into_bytes().size
      );

      // === Materials ===
      if (primitive.material != nullptr) {
        auto& material = *primitive.material;
        ASSERT(material.has_pbr_metallic_roughness);

        auto buf_view = material.pbr_metallic_roughness.base_color_texture.texture->image->buffer_view;
        int x, y, channels;
        u8* mem{};

        mem = stbi_load_from_memory(
            (const stbi_uc*)buf_view->buffer->data + buf_view->offset, (int)buf_view->size, &x, &y, &channels, 4
        );
        ASSERTM(mem != 0, "can't load texture: %s", stbi_failure_reason());

        gpu_mesh.base_color = v.create_image2D(
            vk::image2D::Config{
                .format      = VK_FORMAT_R8G8B8A8_UNORM,
                .extent      = {.constant{.width = (u32)x, .height = (u32)y}},
                .tiling      = VK_IMAGE_TILING_LINEAR, // TODO: use a staging buffer
                .usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .alloc_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            },
            {}
        );
        VK_ASSERT(
            vmaCopyMemoryToAllocation(v.allocator, mem, gpu_mesh.base_color.allocation, 0, VkDeviceSize(4 * x * y))
        );
        stbi_image_free(mem);
      }

      meshes.push(alloc, gpu_mesh);
    }
  };

  auto work_on_node = [&](cgltf_node& node) {
    for (auto& node : core::storage{data->scene->nodes_count, data->scene->nodes}.iter()) {
      if (node->mesh != nullptr) {
        math::Mat4 transform = math::Mat4::Id;
        cgltf_node_transform_world(node, transform._coeffs);
        work_on_mesh(*node->mesh, transform);
      }
    }
  };

  ASSERT(data->scene != nullptr);
  for (auto* node : core::storage{data->scene->nodes_count, data->scene->nodes}.iter()) {
    work_on_node(*node);
  }

  return meshes;
}

void unload_mesh(subsystem::video& v, GpuMesh mesh) {
  vmaDestroyBuffer(v.allocator, mesh.vertex_buffer, mesh.vertex_buf_allocation);
  vmaDestroyBuffer(v.allocator, mesh.index_buffer, mesh.index_buf_allocation);
  mesh.base_color.destroy(v.device, v.allocator);
}
