#include <core/vulkan/subsystem.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <core/containers/vec.h>
#include <core/core.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/init.h>
#include <core/vulkan/vulkan.h>

#include <vk_mem_alloc.h>

core::storage<const char*> enumerate_SDL_vulkan_instance_extensions() {
  u32 instance_extension_count;
  const char** d = (const char**)SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
  return {instance_extension_count, d};
}

vk::image2D swapchain_image_to_image2D(
    VkDevice device,
    vk::Swapchain& swapchain,
    VkImage swapchain_image
) {
  VkImageViewCreateInfo image_view_create_info{
      .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image      = swapchain_image,
      .viewType   = VK_IMAGE_VIEW_TYPE_2D,
      .format     = swapchain.config.surface_format.format,
      .components = {},
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .levelCount = 1,
              .layerCount = 1,
          },
  };
  VkImageView swapchain_image_view;
  VK_ASSERT(vkCreateImageView(device, &image_view_create_info, nullptr, &swapchain_image_view));
  return vk::image2D{
      .source     = vk::image2D::Source::Swapchain,
      .image      = swapchain_image,
      .image_view = swapchain_image_view,
      .extent3    = swapchain.config.extent3,
      .sync =
          {
              .layout = VK_IMAGE_LAYOUT_UNDEFINED,
              .stage  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
              .access = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
          }
  };
}

EXPORT subsystem::video subsystem::init_video(core::Arena& ar) {
  SDL_InitSubSystem(SDL_INIT_VIDEO);

  LOG_INFO("initializing video subsystem");

  vk::init();
  SDL_Window* window =
      SDL_CreateWindow("title", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS);
  ASSERTM(window != nullptr, "can't create SDL window: %s", SDL_GetError());
  auto instance_extensions = enumerate_SDL_vulkan_instance_extensions();
  const char* layers[]     = {
      "VK_LAYER_KHRONOS_validation",
  };
  vk::Instance instance =
      vk::create_default_instance(layers, instance_extensions).expect("can't create instance");
  VkSurfaceKHR surface;
  {
    SDL_bool surface_created = SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);
    ASSERTM(surface_created == SDL_TRUE, "can't create surface: %s", SDL_GetError());
  }

  vk::Device device =
      vk::create_default_device(instance, surface, {}).expect("can't create device");
  vk::Swapchain swapchain =
      vk::create_default_swapchain(ar, device, surface).expect("can't acquire swapchain");

  vk::FrameSynchro frame_sync                  = vk::create_frame_synchro(ar, device, 1);
  VmaAllocatorCreateInfo allocator_create_info = {
      // .flags            = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
      .physicalDevice   = device.physical,
      .device           = device,
      .instance         = instance,
      .vulkanApiVersion = VK_API_VERSION_1_3,
  };
  VmaAllocator allocator;
  VK_ASSERT(vmaCreateAllocator(&allocator_create_info, &allocator));

  core::vec<vk::image2D> swapchain_images{};
  for (auto swapchain_image : swapchain.images.iter()) {
    swapchain_images.push(ar, swapchain_image_to_image2D(device, swapchain, swapchain_image));
  }

  return {
      window, instance, device, surface, swapchain, frame_sync, allocator, swapchain_images,
  };
}

EXPORT void subsystem::uninit_video(video& v) {
  for (auto swapchain_image : v.swapchain_images.iter()) {
    vkDestroyImageView(v.device, swapchain_image.image_view, nullptr);
  }
  vmaDestroyAllocator(v.allocator);
  vk::destroy_frame_synchro(v.device, v.sync);
  vk::destroy_swapchain(v.device, v.swapchain);
  vkDestroySurfaceKHR(v.instance, v.surface, nullptr);
  vk::destroy_device(v.device);
  vk::destroy_instance(v.instance);
  SDL_DestroyWindow(v.window);
}

EXPORT void subsystem::video_rebuild_swapchain(video& v) {
  vk::rebuild_default_swapchain(v.device, v.surface, v.swapchain).expect("can't rebuild swapchain");

  for (auto [idx, swapchain_image] : core::enumerate{v.swapchain_images.iter()}) {
    vkDestroyImageView(v.device, swapchain_image->image_view, nullptr);
    *swapchain_image = swapchain_image_to_image2D(v.device, v.swapchain, v.swapchain.images[idx]);
  }
}

namespace subsystem {
EXPORT vk::Result<VideoFrame> video::begin_frame() {
  auto frame = vk::begin_frame(device, swapchain, sync);
  if (frame.is_err()) {
    return frame.err();
  }

  return {{
      .swapchain_image = swapchain_images[frame->swapchain_image_index],
      .frame           = *frame,
  }};
}

EXPORT VkResult video::end_frame(VideoFrame vframe, VkCommandBuffer cmd) {
  VkSemaphoreSubmitInfo wait_semaphore_submit_info{
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = vframe.frame.acquire_semaphore,
      .stageMask = swapchain_images[vframe.frame.swapchain_image_index].sync.stage,
  };
  VkCommandBufferSubmitInfo cmd_submit_info{
      .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = cmd,
  };
  VkSemaphoreSubmitInfo signal_semaphore_submit_info{
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = vframe.frame.render_semaphore,
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
  };
  VkSubmitInfo2 submit_info{
      .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .waitSemaphoreInfoCount   = 1,
      .pWaitSemaphoreInfos      = &wait_semaphore_submit_info,
      .commandBufferInfoCount   = 1,
      .pCommandBufferInfos      = &cmd_submit_info,
      .signalSemaphoreInfoCount = 1,
      .pSignalSemaphoreInfos    = &signal_semaphore_submit_info
  };
  vkQueueSubmit2(device.omni_queue, 1, &submit_info, vframe.frame.render_done_fence);

  swapchain_images[vframe.frame.swapchain_image_index] = vframe.swapchain_image;
  return vk::end_frame(device, device.omni_queue, swapchain, vframe.frame);
}
} // namespace subsystem