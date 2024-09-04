#ifndef INCLUDE_APP_VULKAN_HELPER_H_
#define INCLUDE_APP_VULKAN_HELPER_H_

#include <core/core.h>
#include <core/vulkan.h>
#include <loader/subsystems.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

struct image2D {
  enum class Source { Swapchain, Created } source;
  VkImage image;
  VmaAllocation allocation;
  VkImageView image_view;

  union {
    VkExtent2D extent2;
    VkExtent3D extent3;
  };

  struct Sync {
    VkImageLayout layout        = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkAccessFlags2 access       = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
  } sync;

  operator VkImage() {
    return image;
  }

  struct Config {
    VkFormat format;
    union image2DExtent {
      enum class tag_t { Constant, Swapchain } tag;
      struct {
        tag_t tag = tag_t::Constant;
        u32 width;
        u32 height;
      } constant;

      struct {
        tag_t tag = tag_t::Swapchain;
      } swapchain;
    } extent = {.constant = {.width = 64, .height = 64}};
    VkImageUsageFlags usage;
  };

  static image2D create(
      subsystem::video& v,
      VmaAllocator allocator,
      const image2D::Config& config,
      Sync sync
  );
  VkImageMemoryBarrier2 sync_to(image2D::Sync new_sync);
  void destroy(subsystem::video& v, VmaAllocator allocator);
};

void full_barrier(VkCommandBuffer cmd);
#endif // INCLUDE_APP_VULKAN_HELPER_H_
