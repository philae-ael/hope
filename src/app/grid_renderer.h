#ifndef INCLUDE_APP_GRID_RENDERER_H_
#define INCLUDE_APP_GRID_RENDERER_H_

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

struct GridPushConstants {
  math::Vec4 base_color{0.5f, 0.5f, 0.5f, 0.8f};
  f32 decay = 10.0f;
  f32 scale = 1.0f;
};

template <>
struct vk::pipeline::PushConstantDescriptor<GridPushConstants> {
  static constexpr core::array ranges{VkPushConstantRange{
      VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(GridPushConstants, base_color), sizeof(GridPushConstants)
  }};
};

struct GridRenderer {
  static inline const core::str8 shader_path = "/assets/shaders/grid.spv"_s;
  VkPipeline pipeline;
  VkPipelineLayout layout;

  static GridRenderer init(
      subsystem::video& v,
      VkFormat color_format,
      VkFormat depth_format,
      VkDescriptorSetLayout camera_descriptor_layout
  ) {
    auto scratch = core::scratch_get();
    defer { scratch.retire(); };
    core::Allocator alloc = scratch;

    auto code = fs::read_all(alloc, shader_path);
    ASSERT(code.size % sizeof(u32) == 0);

    VkShaderModule module = vk::pipeline::ShaderBuilder{code}.build(v.device);

    VkPipelineLayout layout =
        vk::pipeline::PipelineLayoutBuilder{
            .set_layouts          = camera_descriptor_layout,
            .push_constant_ranges = vk::pipeline::PushConstantRanges<GridPushConstants>{}.vk(),
        }
            .build(v.device);

    VkPipeline pipeline = vk::pipeline::PipelineBuilder{
        .rendering = {.color_attachment_formats = {1, &color_format}, .depth_attachment_format = depth_format},
        .shader_stages =
            core::array{
                vk::pipeline::ShaderStage{VK_SHADER_STAGE_VERTEX_BIT, module, "main"}.vk(),
                vk::pipeline::ShaderStage{VK_SHADER_STAGE_FRAGMENT_BIT, module, "main"}.vk()
            },
        .vertex_input  = {},
        .rasterization = {.cull_back = false},
        .depth_stencil = vk::pipeline::DepthStencil::CompareDepth,
        .color_blend   = {core::array{vk::pipeline::ColorBlendAttachement::AlphaBlend.vk()}},
        .dynamic_state = {core::array{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
    }.build(v.device, layout);

    vkDestroyShaderModule(v.device, module, nullptr);
    return {pipeline, layout};
  }

  core::storage<const core::str8> file_deps() {
    return shader_path;
  }

  void render(VkDevice device, VkCommandBuffer cmd, VkDescriptorSet camera_descriptor_set) {
    static vk::PushConstantUploader<GridPushConstants> push_constants{{}};
    utils::config_f32xN("grid.base_color", push_constants.c.base_color._coeffs, 4);
    utils::config_f32("grid.decay", &push_constants.c.decay);
    utils::config_f32("grid.scale", &push_constants.c.scale);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    push_constants.upload(cmd, layout);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &camera_descriptor_set, 0, nullptr);
    vkCmdDraw(cmd, 6, 1, 0, 0);
  }

  void uninit(VkDevice device) {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
  }
};

#endif // INCLUDE_APP_GRID_RENDERER_H_
