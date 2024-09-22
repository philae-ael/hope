#ifndef INCLUDE_APP_TRIANGLE_RENDERER_CPP_
#define INCLUDE_APP_TRIANGLE_RENDERER_CPP_

#include "mesh.h"

#include "core/vulkan/image.h"
#include <core/vulkan.h>
#include <core/vulkan/subsystem.h>
#include <vulkan/vulkan_core.h>

struct AppState;

struct TriangleRenderer {
  VkDescriptorPool descriptor_pool;
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;

  static TriangleRenderer init(subsystem::video& v, VkFormat format);

  static core::storage<core::str8> file_deps();

  void render(AppState* app_state, VkCommandBuffer cmd, vk::image2D target, core::storage<GpuMesh> meshes);
  void uninit(subsystem::video& v);
};

#endif // INCLUDE_APP_TRIANGLE_RENDERER_CPP_
