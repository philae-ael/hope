#ifndef INCLUDE_VULKAN_TIMINGS_H_
#define INCLUDE_VULKAN_TIMINGS_H_

#include "core/core/fwd.h"
#include "vulkan.h"

namespace vk {
struct timestamp_scope {
  core::hstr8 name;
  u64 idx;
};

void timestamp_init(VkDevice device, f32 timestampPeriod);
void timestamp_uninit(VkDevice device);

void timestamp_frame_start(VkDevice device);

timestamp_scope timestamp_scope_start(
    VkCommandBuffer cmd,
    VkPipelineStageFlags2 stage,
    core::hstr8 name
);
void timestamp_scope_end(VkCommandBuffer cmd, VkPipelineStageFlags2 stage, timestamp_scope sc);
} // namespace vk

#endif // INCLUDE_VULKAN_TIMINGS_H_
