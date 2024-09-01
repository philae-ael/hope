#include "core/core.h"

#include "app_loader.h"
#include "subsystems.h"
#include "vulkan/frame.h"
#include "vulkan/vulkan.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <csignal>

using namespace core::enum_helpers;
extern "C" {
EXPORT AppEvent handle_events(SDL_Event& ev) {
  using namespace core::enum_helpers;
  // auto& = *static_cast<>(userdata);

  AppEvent system_event{};
  switch (ev.type) {
  case SDL_QUIT: {
    system_event |= AppEvent::Exit;
    break;
  }
  case SDL_WINDOWEVENT: {
    switch (ev.window.event) {
    case SDL_WINDOWEVENT_CLOSE:
      system_event |= AppEvent::Exit;
      break;
    }

    break;
  }
  case SDL_KEYDOWN: {
    switch (ev.key.keysym.sym) {
    case (SDLK_q): {
      system_event |= AppEvent::Exit;
      break;
    }
    case (SDLK_r): {
      system_event |= AppEvent::RebuildRenderer;
      break;
    }
    case (SDLK_ESCAPE): {
      LOG_INFO("reloading app asked");
      system_event |= AppEvent::ReloadApp;
      break;
    }
    }
    break;
  }
  }
  return system_event;
}

struct Renderer {
  VkCommandPool command_pool;
  VkCommandBuffer cmd;
};

EXPORT Renderer* init_renderer(core::Arena& ar, subsystem::video& v) {
  Renderer* rdata = ar.allocate<Renderer>();

  VkCommandPoolCreateInfo command_pool_create_info{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = v.device.omni_queue_family_index,
  };
  VK_ASSERT(vkCreateCommandPool(v.device, &command_pool_create_info, nullptr, &rdata->command_pool)
  );
  VkCommandBufferAllocateInfo command_pool_allocate_info{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = rdata->command_pool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VK_ASSERT(vkAllocateCommandBuffers(v.device, &command_pool_allocate_info, &rdata->cmd));
  return rdata;
}

EXPORT void uninit_renderer(subsystem::video& v, Renderer* renderer) {
  auto [command_pool, cmd] = *renderer;
  vkDeviceWaitIdle(v.device);
  vkFreeCommandBuffers(v.device, command_pool, 1, &cmd);
  vkDestroyCommandPool(v.device, command_pool, nullptr);
}

EXPORT AppEvent render(subsystem::video& v, Renderer* renderer) {
  auto [command_pool, cmd] = *renderer;
  AppEvent sev{};

  auto frame_ = vk::begin_frame(v.device, v.swapchain, v.sync);
  if (frame_.is_err()) {
    switch (frame_.err()) {
    case VK_TIMEOUT:
      return sev;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
      sev |= AppEvent::RebuildSwapchain;
      return sev;
    default:
      VK_ASSERT(frame_.err());
    }
  }
  auto frame              = frame_.value();
  VkImage swapchain_image = v.swapchain.images[frame.swapchain_image_index];

  VkCommandBufferBeginInfo command_buffer_begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &command_buffer_begin_info);
  {
    VkImageMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image         = swapchain_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkDependencyInfoKHR dep{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dep);
  }

  {
    VkClearColorValue clear_color{.float32 = {0.0, 1.0, 1.0, 0.0}};
    VkImageSubresourceRange clear_range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    vkCmdClearColorImage(
        cmd, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &clear_range
    );
  }
  {
    VkImageMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image         = swapchain_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkDependencyInfoKHR dep{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dep);
  }

  vkEndCommandBuffer(cmd);
  VkPipelineStageFlags acquire_dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkSubmitInfo submit_info{
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = &frame.acquire_semaphore,
      .pWaitDstStageMask    = &acquire_dst_stage,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = &frame.render_semaphore,
  };
  vkQueueSubmit(v.device.omni_queue, 1, &submit_info, frame.render_done_fence);
  VkResult res = vk::end_frame(v.device, v.device.omni_queue, v.swapchain, frame);
  switch (res) {
  case VK_ERROR_OUT_OF_DATE_KHR:
  case VK_SUBOPTIMAL_KHR:
    sev |= AppEvent::RebuildSwapchain;
    break;
  default:
    VK_ASSERT(res);
  }
  return sev;
}

EXPORT void uninit() {}
}
