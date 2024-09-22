#ifndef INCLUDE_SRC_SUBSYSTEMS_H_
#define INCLUDE_SRC_SUBSYSTEMS_H_

#include <SDL3/SDL_video.h>

#include <core/containers/vec.h>
#include <core/core/fwd.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/image.h>
#include <core/vulkan/init.h>
#include <core/vulkan/vulkan.h>
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
  VmaAllocator allocator;

  core::vec<vk::image2D> swapchain_images;

  vk::Result<VideoFrame> begin_frame();
  VkResult end_frame(VideoFrame, VkCommandBuffer);
};
video init_video(core::Allocator alloc);
void uninit_video(video& v);
void video_rebuild_swapchain(video& v);

} // namespace subsystem

#endif // INCLUDE_SRC_SUBSYSTEMS_H_
