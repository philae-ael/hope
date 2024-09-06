#ifndef INCLUDE_APP_VULKAN_HELPER_H_
#define INCLUDE_APP_VULKAN_HELPER_H_

#include "vulkan.h"

#include <core/core.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace subsystem {
struct video;
}

namespace vk {
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

  static image2D create(subsystem::video& v, const image2D::Config& config, Sync sync);
  VkImageMemoryBarrier2 sync_to(image2D::Sync new_sync);

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
  void destroy(subsystem::video& v);
};

void full_barrier(VkCommandBuffer cmd);
} // namespace vk
#endif // INCLUDE_APP_VULKAN_HELPER_H_
