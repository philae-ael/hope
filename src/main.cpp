#include "containers/vec.h"
#include "core/core.h"
#include "core/debug.h"
#include "core/log.h"
#include "os/os.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

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
  log_set_global_level(core::LogLevel::Debug);

  vk::init();

  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window = SDL_CreateWindow(
      "title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
      SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS
  );
  ASSERTM(window != nullptr, "can't create SDL window: %s", SDL_GetError());

  auto s = scratch_get();
  defer { s.retire(); };

  const char* layers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  LOG_INFO("initializing Vulkan");
  auto instance_extensions = enumerate_SDL_vulkan_instance_extensions(*s, window);
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
      vk::create_default_swapchain(*s, device, surface).expect("can't acquire swapchain");

  VkSemaphore acquire_sem;
  VkSemaphore render_sem;
  {
    VkSemaphoreCreateInfo sem_create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_ASSERT(vkCreateSemaphore(device, &sem_create_info, nullptr, &acquire_sem));
    VK_ASSERT(vkCreateSemaphore(device, &sem_create_info, nullptr, &render_sem));
  }
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
  VkFence cmd_fence;
  {
    VkFenceCreateInfo fence_create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    vkCreateFence(device, &fence_create_info, nullptr, &cmd_fence);
  }

  LOG_INFO("Vulkan fully initialized");
  bool running = true;
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
    u32 swapchain_image_index;
    vkWaitForFences(device, 1, &cmd_fence, VK_TRUE, 1000000000);
    VkResult next_image_acquisition = vkAcquireNextImageKHR(
        device, swapchain, 10000000, acquire_sem, VK_NULL_HANDLE, &swapchain_image_index
    );
    if (next_image_acquisition == VK_TIMEOUT) {
      continue;
    }
    VK_ASSERT(next_image_acquisition);
    vkResetFences(device, 1, &cmd_fence);

    VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &command_buffer_begin_info);

    VkImageMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
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
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags acquire_dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkSubmitInfo submit_info{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &acquire_sem,
        .pWaitDstStageMask    = &acquire_dst_stage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &render_sem,
    };
    vkQueueSubmit(device.omni_queue, 1, &submit_info, cmd_fence);
    VkPresentInfoKHR present_infos{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &render_sem,
        .swapchainCount     = 1,
        .pSwapchains        = &swapchain.swapchain,
        .pImageIndices      = &swapchain_image_index,
    };
    VK_ASSERT(vkQueuePresentKHR(device.omni_queue, &present_infos));
  }
  LOG_INFO("Exiting...");
  vkDestroySemaphore(device, acquire_sem, nullptr);
  vkDestroySemaphore(device, render_sem, nullptr);
  vk::destroy_swapchain(device, swapchain);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vk::destroy_device(device);
  vk::destroy_instance(instance);

  SDL_DestroyWindow(window);
  SDL_Quit();
  LOG_INFO("Bye");
}
