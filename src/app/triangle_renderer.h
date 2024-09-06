#ifndef INCLUDE_APP_TRIANGLE_RENDERER_CPP_
#define INCLUDE_APP_TRIANGLE_RENDERER_CPP_

#include "core/vulkan/image.h"
#include <core/vulkan.h>
#include <core/vulkan/subsystem.h>
#include <vulkan/vulkan_core.h>

struct TriangleRenderer {
  VkDescriptorPool descriptor_pool;
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
  VkBuffer buffer;
  VmaAllocation buf_allocation;

  static TriangleRenderer init(subsystem::video& v, VkFormat format);

  void render(VkCommandBuffer cmd, vk::image2D target) {
    core::array color_attachments{target.as_attachment(
        vk::image2D::AttachmentLoadOp::Clear{{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}},
        vk::image2D::AttachmentStoreOp::Store
    )};
    VkRenderingInfo rendering_info{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{}, target.extent2},
        .layerCount           = 1,
        .colorAttachmentCount = (u32)color_attachments.size(),
        .pColorAttachments    = color_attachments.data
    };

    vkCmdBeginRendering(cmd, &rendering_info);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkRect2D scissor{{}, target.extent2};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkViewport viewport{0, 0, (f32)target.extent2.width, (f32)target.extent2.height, 0, 1};
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
  }
  void uninit(subsystem::video& v);
};

#endif // INCLUDE_APP_TRIANGLE_RENDERER_CPP_
