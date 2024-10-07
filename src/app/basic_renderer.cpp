#include "basic_renderer.h"
#include "app.h"
#include "app/camera.h"
#include "core/core/memory.h"

#include <core/core.h>
#include <core/fs/fs.h>
#include <core/math.h>
#include <cstring>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/pipeline.h>
#include <engine/utils/time.h>
#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

using namespace core::literals;
using math::Mat4;

core::array deps{
    "/assets/shaders/tri.spv"_s,
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

BasicRenderer BasicRenderer::init(
    subsystem::video& v,
    VkFormat format,
    VkDescriptorSetLayout camera_descriptor_layout,
    VkDescriptorSetLayout gpu_texture_descriptor_layout
) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;

  VkPipelineLayout pipeline_layout = vk::pipeline::PipelineLayoutBuilder{
      .set_layouts          = {core::array{camera_descriptor_layout, gpu_texture_descriptor_layout}},
      .push_constant_ranges = {core::array{
          VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4)},
          VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Mat4), 4}
      }}
  }.build(v.device);

  auto code = fs::read_all(alloc, deps[0]);
  ASSERT(code.size % sizeof(u32) == 0);
  VkShaderModuleCreateInfo fragment_module_create_info{
      .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = code.size,
      .pCode    = (u32*)code.data,
  };
  VkShaderModule module;
  VK_ASSERT(vkCreateShaderModule(v.device, &fragment_module_create_info, nullptr, &module));

  vk::Pipeline pipeline = vk::pipeline::PipelineBuilder{
      .rendering = {.color_attachment_formats = {1, &format}, .depth_attachment_format = VK_FORMAT_D16_UNORM},
      .shader_stages =
          core::array{
              vk::pipeline::ShaderStage{VK_SHADER_STAGE_VERTEX_BIT, module, "main"}.vk(),
              vk::pipeline::ShaderStage{VK_SHADER_STAGE_FRAGMENT_BIT, module, "main"}.vk()
          },
      .vertex_input =
          {
              core::array{vk::pipeline::VertexBinding<Vertex>{0}.vk()},
              vk::pipeline::VertexInputAttributes<Vertex>{0}.vk(),
          },
      .depth_stencil = vk::pipeline::DepthStencil::WriteAndCompareDepth,
      .color_blend   = {core::array{vk::pipeline::ColorBlendAttachement::NoBlend.vk()}},
      .dynamic_state = {core::array{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
  }.build(v.device, pipeline_layout);

  vkDestroyShaderModule(v.device, module, nullptr);
  return {
      pipeline,
  };
}

core::storage<core::str8> BasicRenderer::file_deps() {
  return deps;
}

void BasicRenderer::render(
    AppState* app_state,
    VkDevice device,
    VkCommandBuffer cmd,
    vk::image2D color_target,
    vk::image2D depth_target,
    VkDescriptorSet camera_descriptor_set,
    VkDescriptorSet gpu_texture_descriptor_set,
    core::storage<GpuMesh> meshes
) {
  using namespace core::literals;
  auto triangle_scope = utils::scope_start("triangle"_hs);
  defer { utils::scope_end(triangle_scope); };

  core::array color_attachments{
      color_target.as_attachment(
          vk::image2D::AttachmentLoadOp::Clear{{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}},
          vk::image2D::AttachmentStoreOp::Store
      ),
  };
  const auto depth_attachment = depth_target.as_attachment(
      vk::image2D::AttachmentLoadOp::Clear{{.depthStencil = {1.0, 0}}}, vk::image2D::AttachmentStoreOp::Store

  );

  VkRenderingInfo rendering_info{
      .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea           = {{}, color_target.extent},
      .layerCount           = 1,
      .colorAttachmentCount = (u32)color_attachments.size(),
      .pColorAttachments    = color_attachments.data,
      .pDepthAttachment     = &depth_attachment,
  };
  vkCmdBeginRendering(cmd, &rendering_info);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  core::array descriptor_sets{
      camera_descriptor_set,
      gpu_texture_descriptor_set,
  };
  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, (u32)descriptor_sets.size(), descriptor_sets.data, 0,
      nullptr
  );

  VkRect2D scissor{{}, color_target.extent};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkViewport viewport{
      0, (f32)color_target.extent.height, (f32)color_target.extent.width, -(f32)color_target.extent.height, 0, 1
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  for (auto [i, mesh] : core::enumerate{meshes.iter()}) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertex_buffer, &offset);

    struct {
      Mat4 transform_matrix;
    } matrices = {
        mesh->transform,
    };
    vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrices), &matrices);
    auto idx{u32(i)};
    vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(matrices), 4, &idx);

    vkCmdBindIndexBuffer(cmd, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, mesh->indice_count, 1, 0, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

GpuTextureDescriptor GpuTextureDescriptor::init(subsystem::video& v) {
  core::array<VkDescriptorPoolSize, 1> pool_sizes{
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 50},
  };
  VkDescriptorPoolCreateInfo descriptor_pool_create_info{
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 1,
      .poolSizeCount = (u32)pool_sizes.size(),
      .pPoolSizes    = pool_sizes.data,
  };
  VkDescriptorPool descriptor_pool;
  vkCreateDescriptorPool(v.device, &descriptor_pool_create_info, nullptr, &descriptor_pool);

  core::array descriptor_set_layout_bindings{
      VkDescriptorSetLayoutBinding{
          .binding         = 0,
          .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 50,
          .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = (u32)descriptor_set_layout_bindings.size(),
      .pBindings    = descriptor_set_layout_bindings.data,
  };
  VkDescriptorSetLayout descriptor_set_layout;
  vkCreateDescriptorSetLayout(v.device, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout);

  VkDescriptorSetAllocateInfo descriptor_set_alloc_infos{
      .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool     = descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts        = &descriptor_set_layout,
  };
  VkDescriptorSet descriptor_set;
  vkAllocateDescriptorSets(v.device, &descriptor_set_alloc_infos, &descriptor_set);
  return {
      descriptor_pool,
      descriptor_set_layout,
      descriptor_set,
  };
}

void GpuTextureDescriptor::update(VkDevice device, VkSampler sampler, core::storage<GpuMesh> meshes) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
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
  VkWriteDescriptorSet write{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = set,
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = (u32)image_infos.size(),
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo      = image_infos.data(),
  };
  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void GpuTextureDescriptor::uninit(subsystem::video& v) {
  vkDestroyDescriptorSetLayout(v.device, layout, nullptr);
  vkDestroyDescriptorPool(v.device, pool, nullptr);
}

CameraDescriptor CameraDescriptor::init(subsystem::video& v) {
  core::array<VkDescriptorPoolSize, 1> pool_sizes{
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
  };
  VkDescriptorPoolCreateInfo descriptor_pool_create_info{
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 1,
      .poolSizeCount = (u32)pool_sizes.size(),
      .pPoolSizes    = pool_sizes.data,
  };
  VkDescriptorPool descriptor_pool;
  vkCreateDescriptorPool(v.device, &descriptor_pool_create_info, nullptr, &descriptor_pool);

  core::array descriptor_set_layout_bindings{
      VkDescriptorSetLayoutBinding{
          .binding         = 0,
          .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
      },
  };
  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = (u32)descriptor_set_layout_bindings.size(),
      .pBindings    = descriptor_set_layout_bindings.data,
  };
  VkDescriptorSetLayout descriptor_set_layout;
  vkCreateDescriptorSetLayout(v.device, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout);

  VkDescriptorSetAllocateInfo descriptor_set_alloc_infos{
      .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool     = descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts        = &descriptor_set_layout,
  };
  VkDescriptorSet descriptor_set;
  vkAllocateDescriptorSets(v.device, &descriptor_set_alloc_infos, &descriptor_set);

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

  VkDescriptorBufferInfo buffer_info{
      .buffer = buffer,
      .offset = 0,
      .range  = sizeof(CameraMatrices),
  };
  VkWriteDescriptorSet write{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = descriptor_set,
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = &buffer_info,
  };
  vkUpdateDescriptorSets(v.device, 1, &write, 0, nullptr);
  return {descriptor_pool, descriptor_set_layout, descriptor_set, buffer, allocation, allocation_info};
}

void CameraDescriptor::update(vk::Device& device, CameraMatrices& matrices) {
  memcpy(allocation_info.pMappedData, &matrices, sizeof(CameraMatrices));
}

void CameraDescriptor::uninit(subsystem::video& v) {
  vkDestroyDescriptorSetLayout(v.device, layout, nullptr);
  vkDestroyDescriptorPool(v.device, pool, nullptr);
  vmaDestroyBuffer(v.device.allocator, buffer, allocation);
}
