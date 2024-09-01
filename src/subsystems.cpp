#include "subsystems.h"
#include "containers/vec.h"
#include "core/core.h"

#include "vulkan/frame.h"
#include "vulkan/init.h"
#include "vulkan/vulkan.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>

core::vec<const char*> enumerate_SDL_vulkan_instance_extensions(
    core::Arena& ar,
    SDL_Window* window
) {
  core::vec<const char*> v;
  u32 instance_extension_count;
  SDL_Vulkan_GetInstanceExtensions(window, &instance_extension_count, nullptr);
  v.set_capacity(ar, instance_extension_count);
  SDL_Vulkan_GetInstanceExtensions(window, &instance_extension_count, v.data());
  v.set_size(instance_extension_count);
  return v;
}

EXPORT subsystem::video subsystem::init_video() {
  auto ar = core::scratch_get();
  SDL_InitSubSystem(SDL_INIT_VIDEO);

  LOG_INFO("initializing video subsystem");

  SDL_Window* window = SDL_CreateWindow(
      "title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
      SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS
  );
  ASSERTM(window != nullptr, "can't create SDL window: %s", SDL_GetError());
  auto instance_extensions = enumerate_SDL_vulkan_instance_extensions(*ar, window);
  const char* layers[]     = {
      "VK_LAYER_KHRONOS_validation",
  };
  vk::Instance instance = vk::create_default_instance(core::vec{layers}, instance_extensions)
                              .expect("can't create instance");
  VkSurfaceKHR surface;
  {
    SDL_bool surface_created = SDL_Vulkan_CreateSurface(window, instance, &surface);
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
