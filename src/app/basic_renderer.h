#ifndef INCLUDE_APP_BASIC_RENDERER_CPP_
#define INCLUDE_APP_BASIC_RENDERER_CPP_

#include "app/camera.h"
#include "mesh.h"

#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan.h>
#include <engine/graphics/vulkan/image.h>

struct AppState;

struct CameraDescriptor {
  VkDescriptorPool pool;
  VkDescriptorSetLayout layout;
  VkDescriptorSet set;
  VkBuffer buffer;
  VmaAllocation allocation;

  static CameraDescriptor init(subsystem::video& v);
  void update(vk::Device& device, CameraMatrices& matrices);
  void uninit(subsystem::video& v);

  operator VkDescriptorSet() const {
    return set;
  }
};
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

  static BasicRenderer init(
      subsystem::video& v,
      VkFormat format,
      VkDescriptorSetLayout camera_descriptor_layout,
      VkDescriptorSetLayout gpu_texture_descriptor_layout
  );

  static core::storage<core::str8> file_deps();

  void render(
      AppState* app_state,
      VkDevice device,
      VkCommandBuffer cmd,
      vk::image2D color_target,
      vk::image2D depth_target,
      VkDescriptorSet camera_descriptor_set,
      VkDescriptorSet gpu_texture_descriptor_set,
      core::storage<GpuMesh> meshes
  );
  void uninit(subsystem::video& v);
};

#endif // INCLUDE_APP_BASIC_RENDERER_CPP_
