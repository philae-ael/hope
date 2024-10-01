#ifndef INCLUDE_GRAPHICS_SUBSYSTEMS_H_
#define INCLUDE_GRAPHICS_SUBSYSTEMS_H_

#include "vulkan/frame.h"
#include "vulkan/image.h"
#include "vulkan/init.h"
#include "vulkan/vulkan.h"

#include <SDL3/SDL_video.h>

#include <core/containers/vec.h>
#include <core/core/fwd.h>
#include <vk_mem_alloc.h>

namespace core {
struct Arena;
} // namespace core

core::vec<const char*> enumerate_SDL_vulkan_instance_extensions(core::Arena& ar, SDL_Window* window);

namespace subsystem {

struct VideoFrame {
  vk::image2D swapchain_image;
  vk::Frame frame;
};

struct video {
  SDL_Window* window;
  vk::Instance instance;
  vk::Device device;
  VkSurfaceKHR surface;
  vk::Swapchain swapchain;
  vk::FrameSynchro sync;

  core::vec<vk::image2D> swapchain_images;

  bool wait_frame(u64 timeout = 0);
  core::tuple<core::Maybe<VideoFrame>, bool> begin_frame();
  VkResult end_frame(VideoFrame, VkCommandBuffer);

  inline vk::image2D create_image2D(const vk::image2D::Config& config, vk::image2D::Sync sync) {
    vk::image2D::ConfigExtentValues config_extent_values{.swapchain = swapchain.config.extent};
    return vk::image2D::create(device, config_extent_values, config, sync);
  }
};
video init_video(core::Allocator alloc);
void uninit_video(video& v);
void video_rebuild_swapchain(video& v);

} // namespace subsystem

#endif // INCLUDE_GRAPHICS_SUBSYSTEMS_H_
