#ifndef INCLUDE_VULKAN_FRAME_H_
#define INCLUDE_VULKAN_FRAME_H_

#include "vulkan.h"
#include <core/core/fwd.h>

namespace vk {
struct FrameSynchro {
  u32 inflight;
  u32 frame_id;
  VkSemaphore* acquire_semaphores;
  VkSemaphore* render_semaphores;
  VkFence* render_done_fences;
};

FrameSynchro create_frame_synchro(core::Allocator alloc, VkDevice device, u32 inflight);

void destroy_frame_synchro(VkDevice device, FrameSynchro& frame_synchro);

struct Frame {
  u32 swapchain_image_index;
  VkSemaphore acquire_semaphore;
  VkSemaphore render_semaphore;
  VkFence render_done_fence;
};

bool wait_frame(VkDevice device, FrameSynchro& sync, u64 timeout = 0);
Result<Frame> begin_frame(VkDevice device, VkSwapchainKHR swapchain, FrameSynchro& sync);

VkResult end_frame(VkDevice device, VkQueue present_queue, VkSwapchainKHR swapchain, Frame frame);

} // namespace vk
#endif // INCLUDE_VULKAN_FRAME_H_