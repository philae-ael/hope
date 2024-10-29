#ifndef INCLUDE_APP_GRID_RENDERER_H_
#define INCLUDE_APP_GRID_RENDERER_H_

#include "engine/utils/time.h"
#include <core/core.h>
#include <core/fs/fs.h>
#include <core/math/math.h>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan/pipeline.h>
#include <engine/graphics/vulkan/rendering.h>
#include <engine/graphics/vulkan/vulkan.h>
#include <engine/utils/config.h>
#include <vulkan/vulkan_core.h>

using namespace core::literals;

#define GRID_EXPONENTIAL_FALLOUT 0x1
#define GRID_PROPORTIONAL_WIDTH 0x2

struct GridPushConstants {
  math::Vec4 base_color{1.0f, 1.0f, 1.0f, 0.2f};
  f32 decay      = 6.0f;
  f32 scale      = 1.0f;
  f32 line_width = 2.0f;
  u32 flags      = 0x0;
};

template <>
struct vk::pipeline::PushConstantDescriptor<GridPushConstants> {
  static constexpr core::array ranges{VkPushConstantRange{
      VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(GridPushConstants, base_color), sizeof(GridPushConstants)
  }};
};

struct GridConfig {
  bool enable = true;
  GridPushConstants pc{};
};

struct GridRenderer {
  static inline const core::str8 shader_vert_path = "/assets/shaders/grid.vert.spv"_s;
  static inline const core::str8 shader_frag_path = "/assets/shaders/grid.frag.spv"_s;
  VkPipeline pipeline;
  VkPipelineLayout layout;

  static GridRenderer init(
      subsystem::video& v,
      VkFormat color_format,
      VkFormat depth_format,
      VkDescriptorSetLayout camera_descriptor_layout
  ) {
    auto scratch          = core::scratch_get();
    core::Allocator alloc = scratch;

    auto code_vert = fs::read_all(alloc, shader_vert_path);
    auto code_frag = fs::read_all(alloc, shader_frag_path);
    ASSERT(code_vert.size % sizeof(u32) == 0);
    ASSERT(code_frag.size % sizeof(u32) == 0);

    VkShaderModule module_vert = vk::pipeline::ShaderBuilder{code_vert}.build(v.device);
    VkShaderModule module_frag = vk::pipeline::ShaderBuilder{code_frag}.build(v.device);
    defer {
      vkDestroyShaderModule(v.device, module_vert, nullptr);
      vkDestroyShaderModule(v.device, module_frag, nullptr);
    };

    VkPipelineLayout layout =
        vk::pipeline::PipelineLayoutBuilder{
            .set_layouts          = camera_descriptor_layout,
            .push_constant_ranges = vk::pipeline::PushConstantRanges<GridPushConstants>{}.vk(),
        }
            .build(v.device);

    VkPipeline pipeline = vk::pipeline::PipelineBuilder{
        .rendering = {.color_attachment_formats = {1, &color_format}, .depth_attachment_format = depth_format},
        .shader_stages =
            {vk::pipeline::ShaderStage{VK_SHADER_STAGE_VERTEX_BIT, module_vert, "main"}.vk(),
             vk::pipeline::ShaderStage{VK_SHADER_STAGE_FRAGMENT_BIT, module_frag, "main"}.vk()},
        .vertex_input  = {},
        .rasterization = {.cull_back = false},
        .depth_stencil = vk::pipeline::DepthStencil::CompareDepth,
        .color_blend   = {vk::pipeline::ColorBlendAttachement::AlphaBlend.vk()},
        .dynamic_state = {{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
    }.build(v.device, layout);

    return {pipeline, layout};
  }

  void render(GridConfig& config, VkDevice device, VkCommandBuffer cmd, VkDescriptorSet camera_descriptor_set) {
    auto scope = utils::scope_start("grid"_hs);
    defer { utils::scope_end(scope); };

    utils::config_bool("grid.enabled", &config.enable);
    utils::config_f32xN("grid.base_color", config.pc.base_color._coeffs, 4);
    utils::config_f32("grid.decay", &config.pc.decay);
    utils::config_f32("grid.scale", &config.pc.scale);
    utils::config_f32("grid.linewidth", &config.pc.line_width);

    {
      static const char* const choices[] = {"linear", "exponential"};
      static int fallout_mode;
      utils::config_choice("grid.fallout_mode", &fallout_mode, choices, ARRAY_SIZE(choices));
      config.pc.flags = (config.pc.flags & ~u32(0x1)) | ((fallout_mode == 1) ? GRID_EXPONENTIAL_FALLOUT : 0);
    }

    {
      static const char* const choices[] = {"pixel", "proportional"};
      static int width_mode;
      utils::config_choice("grid.width_mode", &width_mode, choices, ARRAY_SIZE(choices));
      config.pc.flags = (config.pc.flags & ~u32(0x2)) | ((width_mode == 1) ? GRID_PROPORTIONAL_WIDTH : 0);
    }

    if (!config.enable)
      return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vk::PushConstantUploader{config.pc}.upload(cmd, layout);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &camera_descriptor_set, 0, nullptr);
    vkCmdDraw(cmd, 6, 1, 0, 0);
  }

  void uninit(VkDevice device) {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
  }
};

#endif // INCLUDE_APP_GRID_RENDERER_H_
