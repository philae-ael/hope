#ifndef INCLUDE_APP_BASIC_RENDERER_CPP_
#define INCLUDE_APP_BASIC_RENDERER_CPP_

#include "mesh.h"

#include "core/vulkan/image.h"
#include <core/vulkan.h>
#include <core/vulkan/subsystem.h>
#include <vulkan/vulkan_core.h>

struct AppState;

struct GpuTextureDescriptor {
  VkDescriptorPool pool;
  VkDescriptorSetLayout layout;
  VkDescriptorSet set;

  static GpuTextureDescriptor init(subsystem::video& v);
  void update(VkDevice device, VkSampler sampler, core::storage<GpuMesh> meshes);
  void uninit(subsystem::video& v);

  operator VkDescriptorSet() const {
    return set;
  }
};

struct BasicRenderer {
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;

  static BasicRenderer init(subsystem::video& v, VkFormat format, VkDescriptorSetLayout gpu_texture_descriptor_layout);

  static core::storage<core::str8> file_deps();

  void render(
      AppState* app_state,
      VkDevice device,
      VkCommandBuffer cmd,
      vk::image2D color_target,
      vk::image2D depth_target,
      VkDescriptorSet gpu_texture_descriptor_set,
      core::storage<GpuMesh> meshes
  );
  void uninit(subsystem::video& v);
};

#endif // INCLUDE_APP_BASIC_RENDERER_CPP_
