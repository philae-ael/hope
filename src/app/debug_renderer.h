#ifndef INCLUDE_APP_DEBUG_RENDERER_H_
#define INCLUDE_APP_DEBUG_RENDERER_H_

#include <core/core.h>
#include <core/fs/fs.h>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan.h>
#include <engine/graphics/vulkan/pipeline.h>
#include <engine/graphics/vulkan/rendering.h>
#include <engine/utils/time.h>
#include <vulkan/vulkan_core.h>

struct DebugVertex {
  f32 color[4];
  f32 pos_start[3];
  f32 pos_end[3];
  f32 width;
  u32 flags = 0;
};

template <>
struct vk::pipeline::VertexDescription<DebugVertex> {
  static constexpr core::array input_attributes{
      vk::pipeline::InputAttribute{offsetof(DebugVertex, color), VK_FORMAT_R32G32B32A32_SFLOAT},
      vk::pipeline::InputAttribute{offsetof(DebugVertex, pos_start), VK_FORMAT_R32G32B32_SFLOAT},
      vk::pipeline::InputAttribute{offsetof(DebugVertex, pos_end), VK_FORMAT_R32G32B32_SFLOAT},
      vk::pipeline::InputAttribute{offsetof(DebugVertex, width), VK_FORMAT_R32_SFLOAT},
      vk::pipeline::InputAttribute{offsetof(DebugVertex, flags), VK_FORMAT_R32_UINT},
  };
};

struct DebugRenderer {
  static inline const core::str8 shader_frag_path = "/assets/shaders/debug.frag.spv"_s;
  static inline const core::str8 shader_vert_path = "/assets/shaders/debug.vert.spv"_s;
  VkPipeline pipeline;
  VkPipelineLayout layout;

  VkBuffer vertices;
  VmaAllocation allocation;
  u32 vertex_count                         = 0;
  static const inline u32 max_vertex_count = 200;

  static DebugRenderer init(
      subsystem::video& v,
      VkFormat color_format,
      VkFormat depth_format,
      VkDescriptorSetLayout camera_descriptor_layout
  ) {
    auto scratch          = core::scratch_get();
    core::Allocator alloc = scratch;

    auto code_frag = fs::read_all(alloc, shader_frag_path);
    auto code_vert = fs::read_all(alloc, shader_vert_path);
    ASSERT(code_frag.size % sizeof(u32) == 0);
    ASSERT(code_frag.size % sizeof(u32) == 0);

    VkShaderModule module_vert = vk::pipeline::ShaderBuilder{code_vert}.build(v.device);
    VkShaderModule module_frag = vk::pipeline::ShaderBuilder{code_frag}.build(v.device);
    defer {
      vkDestroyShaderModule(v.device, module_vert, nullptr);
      vkDestroyShaderModule(v.device, module_frag, nullptr);
    };

    VkPipelineLayout layout =
        vk::pipeline::PipelineLayoutBuilder{
            .set_layouts = camera_descriptor_layout,
        }
            .build(v.device);

    VkPipeline pipeline =
        vk::pipeline::PipelineBuilder{
            .rendering = {.color_attachment_formats = {1, &color_format}, .depth_attachment_format = depth_format},
            .shader_stages =
                {vk::pipeline::ShaderStage{VK_SHADER_STAGE_VERTEX_BIT, module_vert, "main"}.vk(),
                 vk::pipeline::ShaderStage{VK_SHADER_STAGE_FRAGMENT_BIT, module_frag, "main"}.vk()},
            .vertex_input =
                {
                    vk::pipeline::VertexBinding<DebugVertex>{0, VK_VERTEX_INPUT_RATE_INSTANCE}.vk(),
                    vk::pipeline::VertexInputAttributes<DebugVertex>{0}.vk(),
                },
            .rasterization = {.cull_back = false},
            .depth_stencil = vk::pipeline::DepthStencil::CompareDepth,
            .color_blend   = {vk::pipeline::ColorBlendAttachement::AlphaBlend.vk()},
            .dynamic_state = {{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}},
        }
            .build(v.device, layout);

    VkBufferCreateInfo buffer_create_info{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size                  = max_vertex_count * sizeof(DebugVertex),
        .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &v.device.omni_queue_family_index,
    };
    VmaAllocationCreateInfo allocation_create_info{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    VkBuffer buffer;
    VmaAllocation allocation;
    vmaCreateBuffer(v.device.allocator, &buffer_create_info, &allocation_create_info, &buffer, &allocation, nullptr);

    return {pipeline, layout, buffer, allocation};
  }
  void uninit(vk::Device& device) {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
    vmaDestroyBuffer(device.allocator, vertices, allocation);
  }

  void reset() {
    vertex_count = 0;
  }
  void push_line(vk::Device device, const DebugVertex& vertex) {
    if (vertex_count >= max_vertex_count) {
      LOG_WARNING("Too much debug vertices");
      return;
    }
    vmaCopyMemoryToAllocation(
        device.allocator, &vertex, allocation, vertex_count * sizeof(DebugVertex), sizeof(DebugVertex)
    );
    vertex_count += 1;
  }

  void render(VkDevice device, VkCommandBuffer cmd, VkDescriptorSet camera_descriptor_set) {
    auto scope = utils::scope_start("debug"_hs);
    defer { utils::scope_end(scope); };
    if (vertex_count == 0)
      return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &camera_descriptor_set, 0, nullptr);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertices, &offset);
    vkCmdDraw(cmd, 6, vertex_count, 0, 0);
  }

  void draw_origin_gizmo(vk::Device& device) {
    // === X
    push_line(
        device,
        DebugVertex{
            .color     = {1.0, 0.0, 0.0, 0.8f},
            .pos_start = {0.0, 0.0, 0.0},
            .pos_end   = {1.0, 0.0, 0.0},
            .width     = 0.01f,
        }
    );
    push_line(
        device,
        DebugVertex{
            .color     = {1.0, 0.0, 0.0, 0.8f},
            .pos_start = {1.0, 0.0, 0.0},
            .pos_end   = {0.9f, 0.05f, 0.0},
            .width     = 0.01f,
        }
    );
    push_line(
        device,
        DebugVertex{
            .color     = {1.0, 0.0, 0.0, 0.8f},
            .pos_start = {1.0, 0.0, 0.0},
            .pos_end   = {0.9f, -0.05f, 0.0},
            .width     = 0.01f,
        }
    );

    // === Y
    push_line(
        device,
        DebugVertex{
            .color     = {0.0, 1.0, 0.0, 0.8f},
            .pos_start = {0.0, 0.0, 0.0},
            .pos_end   = {0.0, 1.0, 0.0},
            .width     = 0.01f,
        }
    );
    push_line(
        device,
        DebugVertex{
            .color     = {0.0, 1.0, 0.0, 0.8f},
            .pos_start = {0.0, 1.0, 0.0},
            .pos_end   = {0.05f, 0.9f, 0.0},
            .width     = 0.01f,
        }
    );
    push_line(
        device,
        DebugVertex{
            .color     = {0.0, 1.0, 0.0, 0.8f},
            .pos_start = {0.0, 1.0, 0.0},
            .pos_end   = {0.0, 0.9f, 0.05f},
            .width     = 0.01f,
        }
    );

    // === Z
    push_line(
        device,
        DebugVertex{
            .color     = {0.0, 0.0, 1.0, 0.8f},
            .pos_start = {0.0, 0.0, 0.0},
            .pos_end   = {0.0, 0.0, 1.0},
            .width     = 0.02f,
        }
    );
    push_line(
        device,
        DebugVertex{
            .color     = {0.0, 0.0, 1.0, 0.8f},
            .pos_start = {0.0, 0.0, 1.0},
            .pos_end   = {0.0, 0.05f, 0.9f},
            .width     = 0.02f,
        }
    );
    push_line(
        device,
        DebugVertex{
            .color     = {0.0, 0.0, 1.0, 0.8f},
            .pos_start = {0.0, 0.0, 1.0},
            .pos_end   = {0.0, -0.05f, 0.9f},
            .width     = 0.02f,
        }
    );
  }
};

#endif // INCLUDE_APP_DEBUG_RENDERER_H_
