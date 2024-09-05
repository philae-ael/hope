#ifndef INCLUDE_VULKAN_SYNC_H_
#define INCLUDE_VULKAN_SYNC_H_

#include "vulkan.h"
#include <core/core.h>

namespace vk {

template <class T>
static constexpr bool is_memory_barrier = false;

template <>
inline constexpr bool is_memory_barrier<VkImageMemoryBarrier2> = true;
template <>
inline constexpr bool is_memory_barrier<VkBufferMemoryBarrier2> = true;
template <>
inline constexpr bool is_memory_barrier<VkMemoryBarrier2> = true;

inline void pipeline_barrier(
    VkCommandBuffer cmd,
    core::storage<VkImageMemoryBarrier2> image_memory_barrier,
    core::storage<VkBufferMemoryBarrier2> buffer_memory_barrier = {},
    core::storage<VkMemoryBarrier2> memory_barrier              = {}
) {

  VkDependencyInfoKHR dep{
      .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .memoryBarrierCount       = (u32)memory_barrier.size,
      .pMemoryBarriers          = memory_barrier.data,
      .bufferMemoryBarrierCount = (u32)buffer_memory_barrier.size,
      .pBufferMemoryBarriers    = buffer_memory_barrier.data,
      .imageMemoryBarrierCount  = (u32)image_memory_barrier.size,
      .pImageMemoryBarriers     = image_memory_barrier.data,
  };
  vkCmdPipelineBarrier2(cmd, &dep);
}

// MAGIC STUFF (but not too complicated tbf)
namespace detail_ {
template <class T, class... Args>
constexpr usize count_type_occurence = 0;

template <class T, class Arg, class... Args>
constexpr usize count_type_occurence<T, Arg, Args...> = 0 + count_type_occurence<T, Args...>;

template <class T, class... Args>
constexpr usize count_type_occurence<T, T, Args...> = 1 + count_type_occurence<T, Args...>;

template <class... Fs>
struct overload : Fs... {
  using Fs::operator()...;
};

} // namespace detail_

template <usize ImageBarrierCount, usize BufferBarrierCount, usize MemoryBarrierCount>
struct Barriers {
  core::array<VkImageMemoryBarrier2, ImageBarrierCount> im_barriers{};
  core::array<VkBufferMemoryBarrier2, BufferBarrierCount> buf_barriers{};
  core::array<VkMemoryBarrier2, MemoryBarrierCount> mem_barriers{};

  template <class... Args>
  constexpr Barriers(Args... args) {
    detail_::overload work{
        [this, count = 0](VkImageMemoryBarrier2 b) mutable { im_barriers[count++] = b; },
        [this, count = 0](VkBufferMemoryBarrier2 b) mutable { buf_barriers[count++] = b; },
        [this, count = 0](VkMemoryBarrier2 b) mutable { mem_barriers[count++] = b; },
    };
    (work(args), ...);
  }
};

template <class... Args>
Barriers(Args...) -> Barriers<
                      detail_::count_type_occurence<VkImageMemoryBarrier2, Args...>,
                      detail_::count_type_occurence<VkBufferMemoryBarrier2, Args...>,
                      detail_::count_type_occurence<VkMemoryBarrier2, Args...>>;

template <usize ImageBarrierCount, usize BufferBarrierCount, usize MemoryBarrierCount>
inline void pipeline_barrier(
    VkCommandBuffer cmd,
    Barriers<ImageBarrierCount, BufferBarrierCount, MemoryBarrierCount> barriers
) {
  return pipeline_barrier(cmd, barriers.im_barriers, barriers.buf_barriers, barriers.mem_barriers);
}

template <class... Args>
inline void pipeline_barrier(VkCommandBuffer cmd, Args... args)
  requires(is_memory_barrier<Args> && ...)
{
  return pipeline_barrier(cmd, vk::Barriers{args...});
}

} // namespace vk

#endif // INCLUDE_VULKAN_SYNC_H_
