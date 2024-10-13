#ifndef INCLUDE_APP_BASIC_RENDERER_CPP_
#define INCLUDE_APP_BASIC_RENDERER_CPP_

#include "app/camera.h"
#include "mesh.h"

#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan.h>
#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/pipeline.h>

struct CameraDescriptor {
  VkDescriptorPool pool;
  VkDescriptorSetLayout layout;
  VkDescriptorSet set;
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;

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

struct Vertex {
  f32 x, y, z;    // Position
  f32 nx, ny, nz; // Normal
  f32 u, v;       // Texcoord
};

template <>
struct vk::pipeline::VertexDescription<Vertex> {
  static constexpr core::array input_attributes{
      vk::pipeline::InputAttribute{offsetof(Vertex, x), VK_FORMAT_R32G32B32_SFLOAT},
      vk::pipeline::InputAttribute{offsetof(Vertex, nx), VK_FORMAT_R32G32B32_SFLOAT},
      vk::pipeline::InputAttribute{offsetof(Vertex, u), VK_FORMAT_R32G32_SFLOAT},
  };
};

struct PushConstant {
  math::Mat4 transform_matrix;
  u32 mesh_idx;
};

template <>
struct vk::pipeline::PushConstantDescriptor<PushConstant> {
  static constexpr core::array ranges{
      VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstant, transform_matrix), sizeof(math::Mat4)},
      VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstant, mesh_idx), sizeof(u32)}
  };
};

struct BasicRenderer {
  VkPipeline pipeline;
  VkPipelineLayout layout;

  static BasicRenderer init(
      subsystem::video& v,
      VkFormat color_format,
      VkFormat depth_format,
      VkDescriptorSetLayout camera_descriptor_layout,
      VkDescriptorSetLayout gpu_texture_descriptor_layout
  );

  void render(
      VkDevice device,
      VkCommandBuffer cmd,
      VkDescriptorSet camera_descriptor_set,
      VkDescriptorSet gpu_texture_descriptor_set,
      core::storage<GpuMesh> meshes
  );
  void uninit(VkDevice device) {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
  }
};

#endif // INCLUDE_APP_BASIC_RENDERER_CPP_
