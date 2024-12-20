#ifndef INCLUDE_VULKAN_PIPELINE_H_
#define INCLUDE_VULKAN_PIPELINE_H_

#include "vulkan.h"
#include <vulkan/vulkan_core.h>

namespace vk::pipeline {

struct ShaderStage {
  VkShaderStageFlagBits stage;
  VkShaderModule module;
  const char* entry_point;

  inline VkPipelineShaderStageCreateInfo vk() const {
    return {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = stage,
        .module = module,
        .pName  = entry_point,
    };
  }
};

struct ShaderBuilder {
  core::storage<const u8> code;
  VkShaderModule build(VkDevice device) {
    VkShaderModuleCreateInfo fragment_module_create_info{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size,
        .pCode    = (u32*)code.data,
    };
    VkShaderModule module;
    VK_ASSERT(vkCreateShaderModule(device, &fragment_module_create_info, nullptr, &module));
    return module;
  }
};

template <class T>
struct PushConstantDescriptor;

template <class PushConstant>
struct PushConstantRanges {
  inline constexpr auto vk() const {
    return PushConstantDescriptor<PushConstant>::ranges;
  }
};

struct InputAttribute {
  u32 offset;
  VkFormat format;
};
template <class Vertex>
struct VertexDescription;

template <class T>
struct VertexBinding {
  u32 binding;
  VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
  inline VkVertexInputBindingDescription vk() const {
    return {
        .binding   = binding,
        .stride    = sizeof(T),
        .inputRate = input_rate,
    };
  }
};
template <class Vertex>
struct VertexInputAttributes {
  u32 binding;
  inline constexpr auto vk() const {
    core::array<VkVertexInputAttributeDescription, VertexDescription<Vertex>::input_attributes.size()> ret{};
    for (usize i = 0; i < VertexDescription<Vertex>::input_attributes.size(); i++) {
      ret[i] = VkVertexInputAttributeDescription{
          .location = (u32)i,
          .binding  = binding,
          .format   = VertexDescription<Vertex>::input_attributes[i].format,
          .offset   = VertexDescription<Vertex>::input_attributes[i].offset,
      };
    }
    return ret;
  }
};

struct PipelineLayoutBuilder {
  core::storage<const VkDescriptorSetLayout> set_layouts;
  core::storage<const VkPushConstantRange> push_constant_ranges;

  VkPipelineLayout build(VkDevice device) {
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = (u32)set_layouts.size,
        .pSetLayouts            = set_layouts.data,
        .pushConstantRangeCount = (u32)push_constant_ranges.size,
        .pPushConstantRanges    = push_constant_ranges.data,
    };
    VkPipelineLayout layout;
    VK_ASSERT(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &layout));

    return layout;
  }
};

// load shader, compile them if needed, use basic reflection to generate pass infos and pipeline
// layout

struct VertexInput {
  core::storage<const VkVertexInputBindingDescription> vertex_input_binding_descriptions;
  core::storage<const VkVertexInputAttributeDescription> vertex_input_attributes_descriptions;
  inline VkPipelineVertexInputStateCreateInfo vk() const {
    return {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = (u32)vertex_input_binding_descriptions.size,
        .pVertexBindingDescriptions      = vertex_input_binding_descriptions.data,
        .vertexAttributeDescriptionCount = (u32)vertex_input_attributes_descriptions.size,
        .pVertexAttributeDescriptions    = vertex_input_attributes_descriptions.data,
    };
  }
};

struct InputAssembly {
  VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  inline VkPipelineInputAssemblyStateCreateInfo vk() const {
    return {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = topology,
        .primitiveRestartEnable = false,
    };
  }
};

struct Viewport {
  inline VkPipelineViewportStateCreateInfo vk() const {
    return {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };
  }
};

struct Rasterization {
  bool cull_back = true;
  inline VkPipelineRasterizationStateCreateInfo vk() const {
    return {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = false,
        .rasterizerDiscardEnable = false,
        .cullMode                = VkCullModeFlags(cull_back ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE),
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = false,
        .depthBiasConstantFactor = 0.0,
        .depthBiasClamp          = 0.0,
        .depthBiasSlopeFactor    = 1.0,
        .lineWidth               = 1.0,
    };
  }
};

struct Multisample {
  VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
  inline VkPipelineMultisampleStateCreateInfo vk() const {
    return {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples  = sample_count,
        .sampleShadingEnable   = false,
        .alphaToCoverageEnable = false,
        .alphaToOneEnable      = false,
    };
  }
};

struct DepthStencil {
  VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;
  bool depth_test              = false;
  bool depth_write             = false;

  inline VkPipelineDepthStencilStateCreateInfo vk() const {
    return {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = depth_test,
        .depthWriteEnable      = depth_write,
        .depthCompareOp        = depth_compare_op,
        .depthBoundsTestEnable = false,
        .stencilTestEnable     = false,
    };
  }
  static const DepthStencil NoDepthTest;
  static const DepthStencil WriteAndCompareDepth;
  static const DepthStencil CompareDepth;
};
inline const DepthStencil DepthStencil::NoDepthTest{
    .depth_test  = false,
    .depth_write = false,
};
inline const DepthStencil DepthStencil::WriteAndCompareDepth{
    .depth_compare_op = VK_COMPARE_OP_LESS,
    .depth_test       = true,
    .depth_write      = true,
};
inline const DepthStencil DepthStencil::CompareDepth{
    .depth_compare_op = VK_COMPARE_OP_LESS,
    .depth_test       = true,
    .depth_write      = false,
};

struct ColorBlendAttachement {
  bool blend_enable;
  bool premultiplied_alpha = false;
  inline VkPipelineColorBlendAttachmentState vk() const {
    return {
        .blendEnable         = blend_enable,
        .srcColorBlendFactor = premultiplied_alpha ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
  }
  static const ColorBlendAttachement NoBlend;
  static const ColorBlendAttachement AlphaBlend;
  static const ColorBlendAttachement AlphaBlendPremultiplied;
};
inline const ColorBlendAttachement ColorBlendAttachement::NoBlend{.blend_enable = false};
inline const ColorBlendAttachement ColorBlendAttachement::AlphaBlend{.blend_enable = true};
inline const ColorBlendAttachement ColorBlendAttachement::AlphaBlendPremultiplied{
    .blend_enable        = true,
    .premultiplied_alpha = true
};

struct ColorBlend {
  core::storage<const VkPipelineColorBlendAttachmentState> color_blend_attachements;
  inline VkPipelineColorBlendStateCreateInfo vk() const {
    return {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = false,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = (u32)color_blend_attachements.size,
        .pAttachments    = color_blend_attachements.data,
    };
  }
};

struct DynamicState {
  core::storage<const VkDynamicState> dynamic_states;
  inline VkPipelineDynamicStateCreateInfo vk() const {
    return {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (u32)dynamic_states.size,
        .pDynamicStates    = dynamic_states.data,
    };
  }
};

struct Rendering {
  core::storage<const VkFormat> color_attachment_formats;
  VkFormat depth_attachment_format;
  VkFormat stencil_attachment_format;
  inline VkPipelineRenderingCreateInfo vk() const {
    return {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = (u32)color_attachment_formats.size,
        .pColorAttachmentFormats = color_attachment_formats.data,
        .depthAttachmentFormat   = depth_attachment_format,
        .stencilAttachmentFormat = stencil_attachment_format,
    };
  }
};

struct PipelineBuilder {
  Rendering rendering{};
  core::storage<const VkPipelineShaderStageCreateInfo> shader_stages{};
  VertexInput vertex_input{};
  InputAssembly input_assembly{};
  Viewport viewport{};
  Rasterization rasterization{};
  Multisample multisample{};
  DepthStencil depth_stencil{};
  ColorBlend color_blend{};
  DynamicState dynamic_state{};

  inline VkPipeline build(VkDevice device, VkPipelineLayout layout) {
    auto rendering_create_info      = rendering.vk();
    auto vertex_input_create_info   = vertex_input.vk();
    auto input_assembly_create_info = input_assembly.vk();
    auto viewport_create_info       = viewport.vk();
    auto rasterization_create_info  = rasterization.vk();
    auto multisample_create_info    = multisample.vk();
    auto depth_stencil_create_info  = depth_stencil.vk();
    auto color_blend_create_info    = color_blend.vk();
    auto dynamic_state_create_info  = dynamic_state.vk();

    VkGraphicsPipelineCreateInfo graphic_pipeline_create_info{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &rendering_create_info,
        .stageCount          = (u32)shader_stages.size,
        .pStages             = shader_stages.data,
        .pVertexInputState   = &vertex_input_create_info,
        .pInputAssemblyState = &input_assembly_create_info,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewport_create_info,
        .pRasterizationState = &rasterization_create_info,
        .pMultisampleState   = &multisample_create_info,
        .pDepthStencilState  = &depth_stencil_create_info,
        .pColorBlendState    = &color_blend_create_info,
        .pDynamicState       = &dynamic_state_create_info,
        .layout              = layout,
        .renderPass          = VK_NULL_HANDLE,
    };
    VkPipeline pipeline;
    VK_ASSERT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphic_pipeline_create_info, nullptr, &pipeline));
    return pipeline;
  }
};

} // namespace vk::pipeline

#endif // INCLUDE_VULKAN_PIPELINE_H_
