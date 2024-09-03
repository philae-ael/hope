#include "subsystems.h"

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

core::storage<const char*> enumerate_SDL_vulkan_instance_extensions() {
  u32 instance_extension_count;
  const char** d = (const char**)SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
  return {instance_extension_count, d};
}

EXPORT subsystem::video subsystem::init_video() {
  auto ar = core::scratch_get();
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
      vk::create_default_swapchain(*ar, device, surface).expect("can't acquire swapchain");

  vk::FrameSynchro frame_sync = vk::create_frame_synchro(*ar, device, 1);
  return {
      window, instance, device, surface, swapchain, frame_sync,
  };
}
EXPORT void subsystem::uninit_video(video& v) {
  vk::destroy_frame_synchro(v.device, v.sync);
  vk::destroy_swapchain(v.device, v.swapchain);
  vkDestroySurfaceKHR(v.instance, v.surface, nullptr);
  vk::destroy_device(v.device);
  vk::destroy_instance(v.instance);
  SDL_DestroyWindow(v.window);
}
EXPORT void subsystem::video_rebuild_swapchain(video& v) {
  vk::rebuild_default_swapchain(v.device, v.surface, v.swapchain).expect("can't rebuild swapchain");
}
