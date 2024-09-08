
#include "triangle_renderer.h"
#include "core/core.h"
#include "core/fs/fs.h"
#include "core/vulkan/image.h"
#include "core/vulkan/pipeline.h"
#include <core/vulkan/subsystem.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

using namespace core::literals;

core::array vertices{
    core::Vec4{-1, -1, 0},
    core::Vec4{1, -1, 0},
    core::Vec4{0, 1, 0},
};

TriangleRenderer TriangleRenderer::init(subsystem::video& v, VkFormat format) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  VkBuffer buffer;
  VmaAllocation buf_alloc;
  VkBufferCreateInfo buf_create_info{
      .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size                  = sizeof(f32) * 4 * 3,
      .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &v.device.omni_queue_family_index
  };
  VmaAllocationCreateInfo alloc_create_info{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
  };
  vmaCreateBuffer(v.allocator, &buf_create_info, &alloc_create_info, &buffer, &buf_alloc, nullptr);
  vmaCopyMemoryToAllocation(v.allocator, &vertices, buf_alloc, 0, sizeof(vertices));

  core::array<VkDescriptorPoolSize, 0> pool_sizes{};
  VkDescriptorPoolCreateInfo descriptor_pool_create_info{
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 1,
      .poolSizeCount = (u32)pool_sizes.size(),
      .pPoolSizes    = pool_sizes.data,
  };
  VkDescriptorPool descriptor_pool;
  vkCreateDescriptorPool(v.device, &descriptor_pool_create_info, nullptr, &descriptor_pool);

  // # Layout
  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = 0,
      .pSetLayouts            = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges    = nullptr
  };
  VkPipelineLayout pipeline_layout;
  vkCreatePipelineLayout(v.device, &pipeline_layout_create_info, nullptr, &pipeline_layout);

  auto code = fs::read_all(*scratch, "assets/shaders/tri.spv"_s);
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

  core::array vertex_binding_descriptions{VkVertexInputBindingDescription{
      .binding   = 0,
      .stride    = sizeof(f32) * 4,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  }};
  core::array vertex_attribute_descriptions{VkVertexInputAttributeDescription{
      .binding = 0,
      .format  = VK_FORMAT_R32G32B32_SFLOAT,
      .offset  = 0,
  }};

  core::array color_blend_attachments{
      vk::pipeline::ColorBlendAttachement::NoBlend.vk(),
  };
  core::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipeline pipeline =
      vk::pipeline::Pipeline{
          .rendering     = {.color_attachment_formats = {1, &format}},
          .shader_stages = shader_stage_create_infos,
          .vertex_input  = {vertex_binding_descriptions, vertex_attribute_descriptions},
          .color_blend   = {color_blend_attachments},
          .dynamic_state = {dynamic_states},
      }
          .build(v.device, pipeline_layout);

  vkDestroyShaderModule(v.device, module, nullptr);
  return {descriptor_pool, pipeline, pipeline_layout, buffer, buf_alloc};
}
void TriangleRenderer::uninit(subsystem::video& v) {
  vkDestroyPipeline(v.device, pipeline, nullptr);
  vkDestroyPipelineLayout(v.device, pipeline_layout, nullptr);
  vmaDestroyBuffer(v.allocator, buffer, buf_allocation);
  vkDestroyDescriptorPool(v.device, descriptor_pool, nullptr);
}
