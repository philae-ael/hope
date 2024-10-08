#ifndef INCLUDE_VULKAN_RENDERING_H_
#define INCLUDE_VULKAN_RENDERING_H_

#include "engine/graphics/vulkan/pipeline.h"
#include "vulkan.h"

namespace vk {
struct RenderingInfo {
  VkRect2D render_area;
  core::storage<VkRenderingAttachmentInfo> color_attachments;
  core::Maybe<VkRenderingAttachmentInfo> depth_attachment;
  inline VkRenderingInfo vk() {
    return VkRenderingInfo{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = render_area,
        .layerCount           = 1,
        .colorAttachmentCount = (u32)color_attachments.size,
        .pColorAttachments    = color_attachments.data,
        .pDepthAttachment     = depth_attachment.is_some() ? &depth_attachment.value() : nullptr,
    };
  }
  void begin_rendering(VkCommandBuffer cmd) {
    auto rendering_info = vk();
    vkCmdBeginRendering(cmd, &rendering_info);
  }
};

template <class PushConstant>
struct PushConstantUploader {
  PushConstant c;
  void upload(VkCommandBuffer cmd, VkPipelineLayout layout) {
    for (auto range : vk::pipeline::PushConstantDescriptor<PushConstant>::ranges.iter()) {
      vkCmdPushConstants(cmd, layout, range.stageFlags, range.offset, range.size, (u8*)&c + range.offset);
    }
  }
};

} // namespace vk

#endif // INCLUDE_VULKAN_RENDERING_H_
