#include "core/vulkan/timings.h"
#include "core/containers/vec.h"
#include "core/core/string.h"
#include "core/debug/time.h"
#include "vulkan.h"
#include <core/core.h>
#include <vulkan/vulkan_core.h>

#define MAX_TIMESTAMPS 128
static core::array<u64, 2 * MAX_TIMESTAMPS> timestamps_storage{};
struct scope {
  core::hstr8 name;
  u64 start;
  u64 end;
};
static core::array<scope, MAX_TIMESTAMPS / 2> scopes_storage{};
static core::vec scopes{core::storage<scope>{scopes_storage}};
static usize timestamp_index = 0;
static bool record           = false;
static float timestamp_period;

static VkQueryPool query_pool_timestamps;

namespace vk {

EXPORT void timestamp_init(VkDevice device, f32 timestampPeriod) {
  VkQueryPoolCreateInfo query_pool_info{
      .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType  = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = MAX_TIMESTAMPS
  };
  vkCreateQueryPool(device, &query_pool_info, nullptr, &query_pool_timestamps);

  timestamp_period = timestampPeriod;
}

EXPORT void timestamp_uninit(VkDevice device) {
  vkDestroyQueryPool(device, query_pool_timestamps, nullptr);
}

EXPORT void timestamp_frame_start(VkDevice device) {
  if (timestamp_index > 0) {
    vkGetQueryPoolResults(
        device, query_pool_timestamps, 0, u32(timestamp_index),
        u32(timestamp_index) * 2 * sizeof(uint64_t), timestamps_storage.data, 2 * sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
    );
  }
  record = true;
  for (auto i : core::range{0uz, timestamp_index}.iter()) {
    if (timestamps_storage[2 * i + 1] == 0) {
      record = false;
      break;
    }
  }

  if (record) {
    for (auto& scope : scopes.iter()) {
      auto t = f32(timestamps_storage[2 * scope.end] - timestamps_storage[2 * scope.start]) *
               timestamp_period;
      debug::scope_import(debug::scope_category::GPU, scope.name, {u64(t)});
    }
    debug::frame_start(debug::scope_category::GPU);
    timestamp_index = 0;
    scopes.set_size(0);
    vkResetQueryPool(device, query_pool_timestamps, 0, MAX_TIMESTAMPS);
  }
}

u64 timestamp_push(VkCommandBuffer cmd, VkPipelineStageFlags2 stage) {
  if (!record) {
    return 0;
  }
  auto idx = timestamp_index++;
  vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_timestamps, (u32)idx);
  return idx;
}

EXPORT timestamp_scope
timestamp_scope_start(VkCommandBuffer cmd, VkPipelineStageFlags2 stage, core::hstr8 name) {
  return {core::intern(name), timestamp_push(cmd, stage)};
}
EXPORT void timestamp_scope_end(
    VkCommandBuffer cmd,
    VkPipelineStageFlags2 stage,
    timestamp_scope sc
) {
  if (record) {
    scopes.push(
        core::noalloc,
        {
            sc.name,
            sc.idx,
            timestamp_push(cmd, stage),
        }
    );
  }
}
} // namespace vk
