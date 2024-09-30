#include "frame.h"
#include "vulkan.h"
#include <engine/utils/time.h>

#include <core/core.h>

using namespace core::literals;

namespace vk {
EXPORT void destroy_frame_synchro(VkDevice device, FrameSynchro& frame_synchro) {
  for (usize idx : core::range<u32>{0, frame_synchro.inflight}.iter()) {
    vkDestroySemaphore(device, frame_synchro.acquire_semaphores[idx], nullptr);
    vkDestroySemaphore(device, frame_synchro.render_semaphores[idx], nullptr);
    vkDestroyFence(device, frame_synchro.render_done_fences[idx], nullptr);
  }
}

EXPORT bool wait_frame(VkDevice device, FrameSynchro& sync, u64 timeout) {
  auto wait_for_fence_scope = utils::scope_start("wait for fence"_hs);
  defer { utils::scope_end(wait_for_fence_scope); };

  auto frame_id = (sync.frame_id + 1) % sync.inflight;
  VkResult res  = vkWaitForFences(device, 1, &sync.render_done_fences[frame_id], VK_TRUE, timeout);

  if (res == VK_TIMEOUT) {
    return false;
  }
  return true;
}

EXPORT Result<Frame> begin_frame(VkDevice device, VkSwapchainKHR swapchain, FrameSynchro& sync) {
  auto frame_id = (sync.frame_id + 1) % sync.inflight;

  u32 swapchain_image_index;
  wait_frame(device, sync, 0);

  VkResult res = vkAcquireNextImageKHR(
      device, swapchain, 0, sync.acquire_semaphores[frame_id], VK_NULL_HANDLE, &swapchain_image_index
  );
  if (res != VK_SUCCESS) {
    return res;
  }
  vkResetFences(device, 1, &sync.render_done_fences[frame_id]);
  sync.frame_id = frame_id;
  return {{
      swapchain_image_index,
      sync.acquire_semaphores[frame_id],
      sync.render_semaphores[frame_id],
      sync.render_done_fences[frame_id],
  }};
}

EXPORT VkResult end_frame(VkDevice device, VkQueue present_queue, VkSwapchainKHR swapchain, Frame frame) {
  auto present = utils::scope_start("present"_hs);
  defer { utils::scope_end(present); };
  VkPresentInfoKHR present_infos{
      .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores    = &frame.render_semaphore,
      .swapchainCount     = 1,
      .pSwapchains        = &swapchain,
      .pImageIndices      = &frame.swapchain_image_index,
  };
  return vkQueuePresentKHR(present_queue, &present_infos);
}
EXPORT FrameSynchro create_frame_synchro(core::Allocator alloc, VkDevice device, u32 inflight) {
  FrameSynchro frame_synchro{
      inflight,
      0,
      alloc.allocate_array<VkSemaphore>(inflight).data,
      alloc.allocate_array<VkSemaphore>(inflight).data,
      alloc.allocate_array<VkFence>(inflight).data,

  };
  for (usize idx : core::range<u32>{0, inflight}.iter()) {
    VkSemaphoreCreateInfo sem_create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_ASSERT(vkCreateSemaphore(device, &sem_create_info, nullptr, &frame_synchro.acquire_semaphores[idx]));
    VK_ASSERT(vkCreateSemaphore(device, &sem_create_info, nullptr, &frame_synchro.render_semaphores[idx]));

    VkFenceCreateInfo fence_create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_ASSERT(vkCreateFence(device, &fence_create_info, nullptr, &frame_synchro.render_done_fences[idx]));
  }
  return frame_synchro;
}
} // namespace vk