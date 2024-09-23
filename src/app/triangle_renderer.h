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
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSet descriptor_set;
  VkSampler sampler;

  static TriangleRenderer init(subsystem::video& v, VkFormat format);

  static core::storage<core::str8> file_deps();

  void render(
      AppState* app_state,
    VkDevice device,
      VkCommandBuffer cmd,
      vk::image2D color,
      vk::image2D depth,
      core::storage<GpuMesh> meshes
  );
  void uninit(subsystem::video& v);
};

#endif // INCLUDE_APP_TRIANGLE_RENDERER_CPP_
