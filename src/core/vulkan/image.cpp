#include "image.h"

#include "subsystem.h"

namespace vk {
EXPORT image2D image2D::create(subsystem::video& v, const Config& config, Sync sync) {
  image2D image{
      .source = Source::Created,
      .sync   = sync,
  };
  VkExtent3D extent{0, 0, 1};
  switch (config.extent.tag) {
  case Config::image2DExtent::tag_t::Constant:
    extent.width  = config.extent.constant.width;
    extent.height = config.extent.constant.height;
    break;
  case Config::image2DExtent::tag_t::Swapchain:
    extent = v.swapchain.config.extent3;
    break;
  }
  image.extent3 = extent;

  VkImageCreateInfo image_create_info{
      .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType   = VK_IMAGE_TYPE_2D,
      .format      = config.format,
      .extent      = extent,
      .mipLevels   = 1,
      .arrayLayers = 1,
      .samples     = VK_SAMPLE_COUNT_1_BIT,
      .usage       = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &v.device.omni_queue_family_index,
      .initialLayout         = sync.layout,
  };
  VmaAllocationCreateInfo alloc_create_info{
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
  };
  VK_ASSERT(vmaCreateImage(
      v.allocator, &image_create_info, &alloc_create_info, &image.image, &image.allocation, nullptr
  ));
  VkImageViewCreateInfo image_view_create_info{
      .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image      = image.image,
      .viewType   = VK_IMAGE_VIEW_TYPE_2D,
      .format     = config.format,
      .components = {},
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .levelCount = 1,
              .layerCount = 1,
          },
  };
  VK_ASSERT(vkCreateImageView(v.device, &image_view_create_info, nullptr, &image.image_view));
  return image;
}

EXPORT VkImageMemoryBarrier2 image2D::sync_to(image2D::Sync new_sync) {
  image2D::Sync old_sync = sync;
  sync                   = new_sync;

  return {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask  = old_sync.stage,
      .srcAccessMask = old_sync.access,
      .dstStageMask  = new_sync.stage,
      .dstAccessMask = new_sync.access,
      .oldLayout     = old_sync.layout,
      .newLayout     = new_sync.layout,
      .image         = image,
      .subresourceRange =
          {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1},
  };
}

EXPORT void image2D::destroy(subsystem::video& v) {
  ASSERT(source == image2D::Source::Created);
  vkDestroyImageView(v.device, image_view, nullptr);
  vmaDestroyImage(v.allocator, image, allocation);
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
      .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .memoryBarrierCount = 1,
      .pMemoryBarriers    = &barrier
  };
  vkCmdPipelineBarrier2(cmd, &dep_info);
}
} // namespace vk
