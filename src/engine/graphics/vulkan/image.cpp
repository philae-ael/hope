#include "image.h"

#include "init.h"
#include <vulkan/vulkan_core.h>

namespace vk {

EXPORT image2D image2D::create(
    const vk::Device& device,
    const ConfigExtentValues& config_extent_values,
    const Config& config,
    Sync sync
) {
  image2D image{
      .source = Source::Created,
      .format = config.format,
      .sync   = sync,
  };
  VkExtent3D extent{0, 0, 1};
  switch (config.extent.tag) {
  case Config::image2DExtent::tag_t::Constant:
    extent.width  = config.extent.constant.width;
    extent.height = config.extent.constant.height;
    break;
  case Config::image2DExtent::tag_t::Swapchain:
    extent = config_extent_values.swapchain;
    break;
  }
  image.extent.extent3 = extent;

  VkImageCreateInfo image_create_info{
      .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType             = VK_IMAGE_TYPE_2D,
      .format                = config.format,
      .extent                = extent,
      .mipLevels             = 1,
      .arrayLayers           = 1,
      .samples               = VK_SAMPLE_COUNT_1_BIT,
      .tiling                = config.tiling,
      .usage                 = config.usage,
      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &device.omni_queue_family_index,
      .initialLayout         = sync.layout,
  };
  VK_ASSERT(vmaCreateImage(
      device.allocator, &image_create_info, &config.alloc_create_info, &image.image, &image.allocation, nullptr
  ));
  vmaSetAllocationName(device.allocator, image.allocation, "image");
  VkImageViewCreateInfo image_view_create_info{
      .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image      = image.image,
      .viewType   = VK_IMAGE_VIEW_TYPE_2D,
      .format     = config.format,
      .components = {},
      .subresourceRange =
          {
              .aspectMask = config.image_view_aspect,
              .levelCount = 1,
              .layerCount = 1,
          },
  };
  VK_ASSERT(vkCreateImageView(device, &image_view_create_info, nullptr, &image.image_view));
  return image;
}

EXPORT VkImageMemoryBarrier2 image2D::sync_to(image2D::Sync new_sync, VkImageAspectFlags aspect_mask) {
  image2D::Sync old_sync = sync;
  sync                   = new_sync;

  return {
      .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask     = old_sync.stage,
      .srcAccessMask    = old_sync.access,
      .dstStageMask     = new_sync.stage,
      .dstAccessMask    = new_sync.access,
      .oldLayout        = old_sync.layout,
      .newLayout        = new_sync.layout,
      .image            = image,
      .subresourceRange = {.aspectMask = aspect_mask, .levelCount = 1, .layerCount = 1},
  };
}

EXPORT void image2D::destroy(const vk::Device& device) {
  if (source == image2D::Source::Nop)
    return;

  ASSERT(source == image2D::Source::Created);
  vkDestroyImageView(device, image_view, nullptr);
  vmaDestroyImage(device.allocator, image, allocation);
  image      = VK_NULL_HANDLE;
  image_view = VK_NULL_HANDLE;
  allocation = VK_NULL_HANDLE;
}
EXPORT void full_barrier(VkCommandBuffer cmd) {
  LOG_WARNING("full barrier");
  VkMemoryBarrier2 barrier{
      .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
      .dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR
  };

  VkDependencyInfo dep_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier
  };
  vkCmdPipelineBarrier2(cmd, &dep_info);
}

EXPORT VkRenderingAttachmentInfo image2D::as_attachment(AttachmentLoadOp loadop, AttachmentStoreOp storeop) {
  return VkRenderingAttachmentInfo{
      .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView   = image_view,
      .imageLayout = sync.layout,
      .loadOp      = loadop.load_op,
      .storeOp     = storeop.store_op,
      .clearValue  = loadop.clear_value,
  };
}
} // namespace vk
