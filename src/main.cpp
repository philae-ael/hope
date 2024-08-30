#include "containers/vec.h"
#include "core/core.h"
#include "core/debug.h"
#include "core/log.h"
#include "core/memory.h"
#include "os/os.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include "vulkan/frame.h"
#include "vulkan/init.h"
#include "vulkan/vulkan.h"

using namespace core;
using namespace core::literals;

log_entry timed_formatter(void* u, Arena& arena, core::log_entry entry) {
  entry         = log_fancy_formatter(nullptr, arena, entry);
  entry.builder = string_builder{}
                      .push(arena, os::time_monotonic(), os::TimeFormat::MM_SS_MMM_UUU_NNN)
                      .push(arena, " ")
                      .append(entry.builder);
  return entry;
}

vec<const char*> enumerate_SDL_vulkan_instance_extensions(Arena& ar, SDL_Window* window) {
  vec<const char*> v;
  u32 instance_extension_count;
  SDL_Vulkan_GetInstanceExtensions(window, &instance_extension_count, nullptr);
  v.set_capacity(ar, instance_extension_count);
  SDL_Vulkan_GetInstanceExtensions(window, &instance_extension_count, v.data());
  v.set_size(instance_extension_count);
  return v;
}

int main(int argc, char* argv[]) {
  setup_crash_handler();
  log_register_global_formatter(timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Warning);

  vk::init();

  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window = SDL_CreateWindow(
      "title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
      SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS
  );
  ASSERTM(window != nullptr, "can't create SDL window: %s", SDL_GetError());

  auto ar = arena_alloc();
  defer { arena_dealloc(ar); };

  const char* layers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  LOG_INFO("initializing Vulkan");
  auto instance_extensions = enumerate_SDL_vulkan_instance_extensions(*ar, window);
  vk::Instance instance =
      vk::create_default_instance(vec{layers}, instance_extensions).expect("can't create instance");
  VkSurfaceKHR surface;
  {
    SDL_bool surface_created = SDL_Vulkan_CreateSurface(window, instance, &surface);
    ASSERTM(surface_created == SDL_TRUE, "can't create surface: %s", SDL_GetError());
  }

  vk::Device device =
      vk::create_default_device(instance, surface, {}).expect("can't create device");
  vk::Swapchain swapchain =
      vk::create_default_swapchain(*ar, device, surface).expect("can't acquire swapchain");

  vk::FrameSynchro sync = vk::create_frame_synchro(*ar, device, 2);

  vk::begin_frame(device, swapchain, sync);

  VkCommandPool command_pool;
  {
    VkCommandPoolCreateInfo command_pool_create_info{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device.omni_queue_family_index,
    };
    VK_ASSERT(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));
  }
  VkCommandBuffer cmd;
  {
    VkCommandBufferAllocateInfo command_pool_allocate_info{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_ASSERT(vkAllocateCommandBuffers(device, &command_pool_allocate_info, &cmd));
  }

  LOG_INFO("Vulkan fully initialized");
  bool running                   = true;
  bool require_swapchain_rebuild = true;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
      case SDL_QUIT: {
        running = false;
        break;
      }
      case SDL_WINDOWEVENT: {
        switch (ev.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
          running = false;
          break;
        }

        break;
      }
      }
    }

    if (require_swapchain_rebuild) {
      vk::rebuild_default_swapchain(device, surface, swapchain).expect("can't rebuild swapchain");
      require_swapchain_rebuild = false;
    }

    auto swapchain_image_index_ = vk::begin_frame(device, swapchain, sync);
    if (swapchain_image_index_.is_err()) {
      switch (swapchain_image_index_.err()) {
      case VK_TIMEOUT:
        continue;
      case VK_ERROR_OUT_OF_DATE_KHR:
      case VK_SUBOPTIMAL_KHR:
        require_swapchain_rebuild = true;
        continue;
      default:
        VK_ASSERT(swapchain_image_index_.err());
      }
    }
    u32 swapchain_image_index = swapchain_image_index_.value();

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
          .image         = swapchain.images[swapchain_image_index],
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
          cmd, swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          &clear_color, 1, &clear_range
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
          .image         = swapchain.images[swapchain_image_index],
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
        .pWaitSemaphores      = &sync.acquire_semaphores[sync.frame_id],
        .pWaitDstStageMask    = &acquire_dst_stage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &sync.render_semaphores[sync.frame_id],
    };
    vkQueueSubmit(device.omni_queue, 1, &submit_info, sync.render_done_fences[sync.frame_id]);
    switch (vk::end_frame(device, device.omni_queue, sync, swapchain, swapchain_image_index)) {
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
      require_swapchain_rebuild = true;
      break;
    default:
      VK_ASSERT(swapchain_image_index_.err());
    }
  }

  LOG_INFO("Exiting...");
  vkDeviceWaitIdle(device);
  vkFreeCommandBuffers(device, command_pool, 1, &cmd);
  vkDestroyCommandPool(device, command_pool, nullptr);
  vk::destroy_frame_synchro(device, sync);
  vk::destroy_swapchain(device, swapchain);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vk::destroy_device(device);
  vk::destroy_instance(instance);

  SDL_DestroyWindow(window);
  SDL_Quit();
  LOG_INFO("Bye");
}
