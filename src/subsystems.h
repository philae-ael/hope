#ifndef INCLUDE_SRC_SUBSYSTEMS_H_
#define INCLUDE_SRC_SUBSYSTEMS_H_

#include "containers/vec.h"
#include "core/fwd.h"
#include "vulkan/frame.h"
#include "vulkan/init.h"
#include "vulkan/vulkan.h"
#include <SDL2/SDL_video.h>

core::vec<const char*> enumerate_SDL_vulkan_instance_extensions(
    core::Arena& ar,
    SDL_Window* window
);

namespace subsystem {

struct video {
  SDL_Window* window;
  vk::Instance instance;
  vk::Device device;
  VkSurfaceKHR surface;
  vk::Swapchain swapchain;
  vk::FrameSynchro sync;
};
video init_video();
void uninit_video(video& v);
void video_rebuild_swapchain(video& v);

} // namespace subsystem

#endif // INCLUDE_SRC_SUBSYSTEMS_H_
