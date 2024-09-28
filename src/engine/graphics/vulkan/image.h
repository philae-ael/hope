#ifndef INCLUDE_APP_VULKAN_HELPER_H_
#define INCLUDE_APP_VULKAN_HELPER_H_

#include "vulkan.h"

#include <core/core.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace vk {
struct Device;

struct image2D {
  enum class Source { Nop, Swapchain, Created } source = Source::Nop;
  VkImage image;
  VmaAllocation allocation;
  VkImageView image_view;

  VkExtent extent;

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
    } extent             = {.constant = {.width = 64, .height = 64}};
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags usage;
    VkImageAspectFlags image_view_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VmaAllocationCreateFlags alloc_flags{};
  };
  struct ConfigExtentValues {
    VkExtent swapchain;
  };

  static image2D create(
      const vk::Device& device,
      const ConfigExtentValues& config_extent_values,
      const image2D::Config& config,
      Sync sync
  );
  VkImageMemoryBarrier2 sync_to(image2D::Sync new_sync, VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

  struct AttachmentLoadOp {
    VkAttachmentLoadOp load_op;
    VkClearValue clear_value{};
    static TAG(Load);
    static TAG(DontCare);
    struct Clear {
      VkClearValue clear_value;
    };

    AttachmentLoadOp(Load_t)
        : load_op(VK_ATTACHMENT_LOAD_OP_LOAD) {}
    AttachmentLoadOp(DontCare_t)
        : load_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE) {}
    AttachmentLoadOp(Clear c)
        : load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
        , clear_value(c.clear_value) {}
  };
  struct AttachmentStoreOp {
    VkAttachmentStoreOp store_op;
    static TAG(Store);
    static TAG(DontCare);

    AttachmentStoreOp(Store_t)
        : store_op(VK_ATTACHMENT_STORE_OP_STORE) {}
    AttachmentStoreOp(DontCare_t)
        : store_op(VK_ATTACHMENT_STORE_OP_STORE) {}
  };

  VkRenderingAttachmentInfo as_attachment(AttachmentLoadOp loadop, AttachmentStoreOp storeop);
  void destroy(const vk::Device& device);
};

void full_barrier(VkCommandBuffer cmd);
} // namespace vk
#endif // INCLUDE_APP_VULKAN_HELPER_H_
