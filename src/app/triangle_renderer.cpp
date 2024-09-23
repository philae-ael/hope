#include "triangle_renderer.h"
#include "app.h"
#include "core/core/memory.h"
#include "core/vulkan/sync.h"

#include <core/core.h>
#include <core/fs/fs.h>
#include <core/math.h>
#include <core/utils/time.h>
#include <core/vulkan/image.h>
#include <core/vulkan/pipeline.h>
#include <core/vulkan/subsystem.h>
#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

using namespace core::literals;
using math::Mat4;

core::array deps{
    "assets/shaders/tri.spv"_s,
};

TriangleRenderer TriangleRenderer::init(subsystem::video& v, VkFormat format) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;

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

  core::array push_constants{
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset     = 0,
          .size       = 3 * 4 * 4 * sizeof(f32), // 3 mat4
      },
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset     = 192,
          .size       = 4,
      },
  };

  // # Layout
  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = 1,
      .pSetLayouts            = &descriptor_set_layout,
      .pushConstantRangeCount = (u32)push_constants.size(),
      .pPushConstantRanges    = push_constants.data,
  };
  VkPipelineLayout pipeline_layout;
  vkCreatePipelineLayout(v.device, &pipeline_layout_create_info, nullptr, &pipeline_layout);

  auto code = fs::read_all(alloc, deps[0]);
  ASSERT(code.size % sizeof(u32) == 0);

  // # Pipeline
  VkShaderModuleCreateInfo fragment_module_create_info{
      .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = code.size,
      .pCode    = (u32*)code.data,
  };
  VkShaderModule module;
  VK_ASSERT(vkCreateShaderModule(v.device, &fragment_module_create_info, nullptr, &module));

  core::array shader_stage_create_infos{
      VkPipelineShaderStageCreateInfo{
          .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_VERTEX_BIT,
          .module = module,
          .pName  = "main"
      },
      VkPipelineShaderStageCreateInfo{
          .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = module,
          .pName  = "main"
      },
  };

  core::array vertex_binding_descriptions{
      VkVertexInputBindingDescription{
          .binding   = 0,
          .stride    = sizeof(f32) * (3 + 3 + 2),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      },
  };
  core::array vertex_attribute_descriptions{
      VkVertexInputAttributeDescription{
          .location = 0,
          .binding  = 0,
          .format   = VK_FORMAT_R32G32B32_SFLOAT,
          .offset   = 0,
      },
      VkVertexInputAttributeDescription{
          .location = 1,
          .binding  = 0,
          .format   = VK_FORMAT_R32G32B32_SFLOAT,
          .offset   = sizeof(f32) * 3,
      },
      VkVertexInputAttributeDescription{
          .location = 2,
          .binding  = 0,
          .format   = VK_FORMAT_R32G32_SFLOAT,
          .offset   = sizeof(f32) * 6,
      },
  };

  core::array color_blend_attachments{
      vk::pipeline::ColorBlendAttachement::NoBlend.vk(),
  };
  core::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipeline pipeline =
      vk::pipeline::Pipeline{
          .rendering =
              {
                  .color_attachment_formats = {1, &format},
                  .depth_attachment_format  = VK_FORMAT_D16_UNORM,
              },
          .shader_stages = shader_stage_create_infos,
          .vertex_input  = {vertex_binding_descriptions, vertex_attribute_descriptions},
          .depth_stencil = vk::pipeline::DepthStencil::WriteAndCompareDepth,
          .color_blend   = {color_blend_attachments},
          .dynamic_state = {dynamic_states},
      }
          .build(v.device, pipeline_layout);

  vkDestroyShaderModule(v.device, module, nullptr);
  VkSamplerCreateInfo sampler_create_info{
      .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter        = VK_FILTER_LINEAR,
      .minFilter        = VK_FILTER_LINEAR,
      .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias       = 0.0,
      .anisotropyEnable = false,
      .maxAnisotropy    = 1.0,
      .compareEnable    = false,
      .borderColor      = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
  };
  VkSampler sampler;
  vkCreateSampler(v.device, &sampler_create_info, nullptr, &sampler);
  return {descriptor_pool, pipeline, pipeline_layout, descriptor_set_layout, descriptor_set, sampler};
}

void TriangleRenderer::uninit(subsystem::video& v) {
  vkDestroySampler(v.device, sampler, nullptr);
  vkDestroyPipeline(v.device, pipeline, nullptr);
  vkDestroyPipelineLayout(v.device, pipeline_layout, nullptr);

  vkDestroyDescriptorSetLayout(v.device, descriptor_set_layout, nullptr);
  vkDestroyDescriptorPool(v.device, descriptor_pool, nullptr);
}

core::storage<core::str8> TriangleRenderer::file_deps() {
  return deps;
}

void TriangleRenderer::render(
    AppState* app_state,
    VkDevice device,
    VkCommandBuffer cmd,
    vk::image2D color,
    vk::image2D depth,
    core::storage<GpuMesh> meshes
) {
  using namespace core::literals;
  auto triangle_scope = utils::scope_start("triangle"_hs);
  defer { utils::scope_end(triangle_scope); };

  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;

  defer { utils::scope_end(triangle_scope); };
  core::vec<VkDescriptorImageInfo> image_infos;

  for (auto& mesh : meshes.iter()) {
    vk::pipeline_barrier(cmd, mesh.base_color.sync_to({VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}));

    image_infos.push(
        alloc,
        VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = mesh.base_color.image_view,
            .imageLayout = mesh.base_color.sync.layout,
        }
    );
  }

  VkWriteDescriptorSet write{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = descriptor_set,
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = (u32)image_infos.size(),
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo      = image_infos.data(),
  };
  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

  core::array color_attachments{
      color.as_attachment(
          vk::image2D::AttachmentLoadOp::Clear{{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}},
          vk::image2D::AttachmentStoreOp::Store
      ),
  };
  const auto depth_attachment = depth.as_attachment(
      vk::image2D::AttachmentLoadOp::Clear{{.depthStencil = {1.0, 0}}}, vk::image2D::AttachmentStoreOp::Store

  );

  VkRenderingInfo rendering_info{
      .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea           = {{}, color.extent2},
      .layerCount           = 1,
      .colorAttachmentCount = (u32)color_attachments.size(),
      .pColorAttachments    = color_attachments.data,
      .pDepthAttachment     = &depth_attachment,
  };
  vkCmdBeginRendering(cmd, &rendering_info);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  VkRect2D scissor{{}, color.extent2};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkViewport viewport{0, (f32)color.extent2.height, (f32)color.extent2.width, -(f32)color.extent2.height, 0, 1};
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  for (auto [i, mesh] : core::enumerate{meshes.iter()}) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertex_buffer, &offset);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

    f32 aspect_ratio = (f32)color.extent2.width / (f32)color.extent2.height;
    struct {
      CameraMatrices camera_matrices;
      Mat4 transform_matrix;
    } matrices = {
        app_state->camera.matrices(aspect_ratio),
        mesh->transform,
    };
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrices), &matrices);

    auto idx{u32(i)};
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(matrices), 4, &idx);

    vkCmdBindIndexBuffer(cmd, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, mesh->indices, 1, 0, 0, 0);
  }

  vkCmdEndRendering(cmd);
}
