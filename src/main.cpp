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

int main(int argc, char* argv[]) {
  setup_crash_handler();
  log_register_global_formatter(log_fancy_formatter, nullptr);
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

  vec<const char*> instance_extensions;
  u32 instance_extension_count;
  SDL_Vulkan_GetInstanceExtensions(window, &instance_extension_count, nullptr);
  instance_extensions.set_capacity(*s, instance_extension_count);
  SDL_Vulkan_GetInstanceExtensions(window, &instance_extension_count, instance_extensions.data());
  instance_extensions.set_size(instance_extension_count);

  vk::Instance instance = vk::create_default_instance(*s, vec{layers}, instance_extensions)
                              .expect("can't create instance");
  VkSurfaceKHR surface;
  {
    SDL_bool surface_created = SDL_Vulkan_CreateSurface(window, instance, &surface);
    ASSERTM(surface_created == SDL_TRUE, "can't create surface: %s", SDL_GetError());
  }

  vk::Device device =
      vk::create_default_device(*s, instance, surface, {}).expect("can't create device");

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
  }

  vkDestroySurfaceKHR(instance, surface, nullptr);
  vk::destroy_device(device);
  vk::destroy_instance(instance);

  SDL_DestroyWindow(window);
  SDL_Quit();
}
