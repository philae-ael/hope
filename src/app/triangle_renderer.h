#ifndef INCLUDE_APP_TRIANGLE_RENDERER_CPP_
#define INCLUDE_APP_TRIANGLE_RENDERER_CPP_

#include "core/vulkan/image.h"
#include <core/vulkan.h>
#include <core/vulkan/subsystem.h>
#include <vulkan/vulkan_core.h>

struct Mesh {
  u32 count;
};

struct TriangleRenderer {
  VkDescriptorPool descriptor_pool;
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
  VkBuffer vertex_buffer;
  VmaAllocation vertex_buf_allocation;
  VkBuffer index_buffer;
  VmaAllocation index_buf_allocation;

  Mesh mesh;

  static TriangleRenderer init(subsystem::video& v, VkFormat format);

  static core::storage<core::str8> file_deps();

  void render(VkCommandBuffer cmd, vk::image2D target);
  void uninit(subsystem::video& v);
};

#endif // INCLUDE_APP_TRIANGLE_RENDERER_CPP_
