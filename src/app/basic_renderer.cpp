#include "basic_renderer.h"
#include "camera.h"

#include <core/core.h>
#include <core/fs/fs.h>
#include <core/math.h>
#include <cstring>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan/descriptor.h>
#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/pipeline.h>
#include <engine/graphics/vulkan/rendering.h>
#include <engine/graphics/vulkan/vulkan.h>
#include <engine/utils/config.h>
#include <engine/utils/time.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

using namespace core::literals;
using math::Mat4;

core::str8 shader_src = "/assets/shaders/tri.spv"_s;

BasicRenderer BasicRenderer::init(
    subsystem::video& v,
    VkFormat color_format,
    VkFormat depth_format,
    VkDescriptorSetLayout camera_descriptor_layout,
    VkDescriptorSetLayout gpu_texture_descriptor_layout
) {
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;

  auto code = fs::read_all(alloc, shader_src);
  ASSERT(code.size % sizeof(u32) == 0);

  VkShaderModule module = vk::pipeline::ShaderBuilder{code}.build(v.device);

  VkPipelineLayout layout = vk::pipeline::PipelineLayoutBuilder{
      .set_layouts          = {camera_descriptor_layout, gpu_texture_descriptor_layout},
      .push_constant_ranges = {vk::pipeline::PushConstantRanges<PushConstant>{}.vk()}
  }.build(v.device);

  VkPipeline pipeline = vk::pipeline::PipelineBuilder{
      .rendering = {.color_attachment_formats = {1, &color_format}, .depth_attachment_format = depth_format},
      .shader_stages =
          {vk::pipeline::ShaderStage{VK_SHADER_STAGE_VERTEX_BIT, module, "main"}.vk(),
           vk::pipeline::ShaderStage{VK_SHADER_STAGE_FRAGMENT_BIT, module, "main"}.vk()},
      .vertex_input =
          {
              {vk::pipeline::VertexBinding<Vertex>{0}.vk()},
              vk::pipeline::VertexInputAttributes<Vertex>{0}.vk(),
          },
      .depth_stencil = vk::pipeline::DepthStencil::WriteAndCompareDepth,
      .color_blend   = {vk::pipeline::ColorBlendAttachement::NoBlend.vk()},
      .dynamic_state = {{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
  }.build(v.device, layout);

  vkDestroyShaderModule(v.device, module, nullptr);
  return {pipeline, layout};
}

void BasicRenderer::render(
    VkDevice device,
    VkCommandBuffer cmd,
    VkDescriptorSet camera_descriptor_set,
    VkDescriptorSet gpu_texture_descriptor_set,
    core::storage<GpuMesh> meshes
) {
  using namespace core::literals;
  auto triangle_scope = utils::scope_start("triangle"_hs);
  defer { utils::scope_end(triangle_scope); };
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  static bool enable = true;
  utils::config_bool("mesh.enable", &enable);
  if (!enable)
    return;

  core::array descriptor_sets{
      camera_descriptor_set,
      gpu_texture_descriptor_set,
  };
  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (u32)descriptor_sets.size(), descriptor_sets.data, 0, nullptr
  );

  // Do render

  for (auto [mesh_idx, mesh] : core::enumerate{meshes.iter()}) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertex_buffer, &offset);

    vk::PushConstantUploader<PushConstant>{
        mesh->transform,
        u32(mesh_idx),
    }
        .upload(cmd, layout);

    vkCmdBindIndexBuffer(cmd, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, mesh->indice_count, 1, 0, 0, 0);
  }
}

GpuTextureDescriptor GpuTextureDescriptor::init(subsystem::video& v) {
  VkDescriptorPool pool = vk::DescriptorPoolBuilder{
      .max_sets = 1, .sizes = {VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50}}
  }.build(v.device);

  VkDescriptorSetLayout layout = vk::DescriptorSetLayoutBuilder{
      .bindings = {VkDescriptorSetLayoutBinding{
          0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50, VK_SHADER_STAGE_FRAGMENT_BIT
      }}
  }.build(v.device);

  VkDescriptorSet set = vk::DescriptorSetAllocateInfo{pool, {layout}}.allocate(v.device);

  return {
      pool,
      layout,
      set,
  };
}

void GpuTextureDescriptor::update(VkDevice device, VkSampler sampler, core::storage<GpuMesh> meshes) {
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;
  core::vec<VkDescriptorImageInfo> image_infos;
  for (auto& mesh : meshes.iter()) {
    image_infos.push(
        alloc,
        VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = mesh.base_color.image_view,
            .imageLayout = mesh.base_color.sync.layout,
        }
    );
  }
  if (image_infos.size() == 0) {
    return;
  }
  vk::DescriptorSetUpdater{
      .write_descriptor_set{
          vk::DescriptorSetWriter{
              .set               = set,
              .dst_binding       = 0,
              .dst_array_element = 0,
              .descriptor_type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .image             = {.image_infos = image_infos},
          }
              .vk(),
      },
  }
      .update(device);
}

void GpuTextureDescriptor::uninit(subsystem::video& v) {
  vkDestroyDescriptorSetLayout(v.device, layout, nullptr);
  vkDestroyDescriptorPool(v.device, pool, nullptr);
}

CameraDescriptor CameraDescriptor::init(subsystem::video& v) {
  VkDescriptorPool pool = vk::DescriptorPoolBuilder{
      .max_sets = 1, .sizes = {VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1}}
  }.build(v.device);

  VkDescriptorSetLayout layout = vk::DescriptorSetLayoutBuilder{
      .bindings = {VkDescriptorSetLayoutBinding{
          0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
      }}
  }.build(v.device);

  VkDescriptorSet set = vk::DescriptorSetAllocateInfo{pool, {layout}}.allocate(v.device);

  // **

  VkBufferCreateInfo buffer_create_info{
      .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size                  = sizeof(CameraMatrices),
      .usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &v.device.omni_queue_family_index,
  };
  VmaAllocationCreateInfo allocation_create_info{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
  };
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
  VK_ASSERT(vmaCreateBuffer(
      v.device.allocator, &buffer_create_info, &allocation_create_info, &buffer, &allocation, &allocation_info
  ));

  vk::DescriptorSetUpdater{
      vk::DescriptorSetWriter{
          .set               = set,
          .dst_binding       = 0,
          .dst_array_element = 0,
          .descriptor_type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .buffer =
              {
                  .buffer_infos = VkDescriptorBufferInfo{buffer, 0, sizeof(CameraMatrices)},
              }
      }.vk(),
  }
      .update(v.device);
  return {pool, layout, set, buffer, allocation, allocation_info};
}

void CameraDescriptor::update(vk::Device& device, CameraMatrices& matrices) {
  memcpy(allocation_info.pMappedData, &matrices, sizeof(CameraMatrices));
}

void CameraDescriptor::uninit(subsystem::video& v) {
  vkDestroyDescriptorSetLayout(v.device, layout, nullptr);
  vkDestroyDescriptorPool(v.device, pool, nullptr);
  vmaDestroyBuffer(v.device.allocator, buffer, allocation);
}
