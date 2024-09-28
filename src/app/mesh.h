#ifndef INCLUDE_APP_MESH_LOADER_H_
#define INCLUDE_APP_MESH_LOADER_H_

#include <core/containers/vec.h>
#include <core/math/math.h>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan.h>

#include <vk_mem_alloc.h>

struct GpuMesh {
  VkBuffer vertex_buffer;
  VkBuffer index_buffer;
  VmaAllocation vertex_buf_allocation;
  VmaAllocation index_buf_allocation;

  vk::image2D base_color;

  math::Mat4 transform = math::Mat4::Id;
  u32 indices;
};

core::vec<GpuMesh> load_mesh(core::Allocator alloc, subsystem::video&, core::str8 src);
void unload_mesh(subsystem::video&, GpuMesh);

#endif // INCLUDE_APP_MESH_LOADER_H_
