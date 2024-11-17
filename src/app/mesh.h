#ifndef INCLUDE_APP_MESH_LOADER_H_
#define INCLUDE_APP_MESH_LOADER_H_

#include "core/core.h"
#include "core/core/sched.h"
#include <core/containers/handle_map.h>
#include <core/containers/vec.h>
#include <core/math/math.h>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

struct StagingBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VkDeviceSize offset;
  VkDeviceSize size;

  static core::Maybe<StagingBuffer> init(vk::Device& device, VkDeviceSize size, VkCommandBuffer cmd) {
    if (size == 0) {
      return StagingBuffer{};
    }

    VkBufferCreateInfo buffer_create_info{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size                  = size,
        .usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &device.omni_queue_family_index,
    };
    VmaAllocationCreateInfo allocation_create_info{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    VkBuffer buffer;
    VmaAllocation allocation;
    VkResult res =
        vmaCreateBuffer(device.allocator, &buffer_create_info, &allocation_create_info, &buffer, &allocation, nullptr);
    if (res != VK_SUCCESS) {
      LOG2_TRACE(res);
      return core::None<StagingBuffer>();
    }
    vmaSetAllocationName(device.allocator, allocation, "staging buffer");

    return StagingBuffer{buffer, allocation, 0, size};
  }

  void cmdCopyMemoryToBuffer(
      vk::Device& device,
      VkCommandBuffer cmd,
      void* src,
      VkBuffer dst_buffer,
      VkDeviceSize dst_offset,
      VkDeviceSize dst_size
  ) {
    ASSERT(enough_memory(dst_size));
    vmaCopyMemoryToAllocation(device.allocator, src, allocation, offset, dst_size);

    VkBufferCopy region{
        .srcOffset = offset,
        .dstOffset = dst_offset,
        .size      = dst_size,
    };
    vkCmdCopyBuffer(cmd, buffer, dst_buffer, 1, &region);

    offset += dst_size;
  }
  bool enough_memory(VkDeviceSize dst_size) const {
    return offset + dst_size < size;
  }
  bool image_enough_memory(usize texel_size, u32 image_width, u32 image_height) const {
    usize dst_size = image_width * image_height * texel_size;
    usize offset_  = ALIGN_UP(offset, texel_size);
    return offset_ + dst_size <= size;
  }

  void cmdCopyMemoryToImage(
      vk::Device& device,
      VkCommandBuffer cmd,
      void* src,
      VkImage dst_image,
      VkImageLayout dst_image_layout,
      VkExtent3D dst_image_extent3,
      usize texel_size,
      VkOffset3D dst_image_offset,
      u32 image_width,
      u32 image_height
  ) {
    usize dst_size = image_width * image_height * texel_size;
    offset         = ALIGN_UP(offset, texel_size);
    ASSERT(image_enough_memory(texel_size, image_width, image_height));
    vmaCopyMemoryToAllocation(device.allocator, src, allocation, offset, dst_size);

    VkBufferImageCopy region{
        .bufferOffset      = offset,
        .bufferRowLength   = image_width,
        .bufferImageHeight = image_height,
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel   = 0,
                .layerCount = 1,
            },
        .imageOffset = dst_image_offset,
        .imageExtent = dst_image_extent3
    };

    vkCmdCopyBufferToImage(cmd, buffer, dst_image, dst_image_layout, 1, &region);

    offset += dst_size;
  }

  void cmdCopyMemoryToImage(
      vk::Device& device,
      VkCommandBuffer cmd,
      void* src,
      vk::image2D& dst,
      usize texel_size,
      VkOffset3D dst_image_offset,
      u32 image_width,
      u32 image_height
  ) {
    cmdCopyMemoryToImage(
        device, cmd, src, dst.image, dst.sync.layout, dst.extent.extent3, texel_size, dst_image_offset, image_width,
        image_height
    );
  }

  void uninit(vk::Device& device) {
    vmaDestroyBuffer(device.allocator, buffer, allocation);
  }
  void reset() {
    offset = 0;
  }
};

struct GpuMesh {
  VkBuffer vertex_buffer;
  VkBuffer index_buffer;
  VmaAllocation vertex_buf_allocation;
  VmaAllocation index_buf_allocation;
  usize base_color_texture_idx;

  math::Mat4 transform = math::Mat4::Id;
  u32 indice_count;

  // if true, indices are u32
  bool huge_indices;
};

void unload_mesh(subsystem::video&, GpuMesh);

struct CommandBuffer {
  VkCommandBuffer cmd;
  VkFence fence;
  bool done(VkDevice device) {
    return vkGetFenceStatus(device, fence) == VK_SUCCESS;
  }

  static CommandBuffer init(VkDevice device, VkCommandPool pool) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocate_info{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    vkAllocateCommandBuffers(device, &allocate_info, &cmd);

    VkFence fence;
    VkFenceCreateInfo fence_create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    vkCreateFence(device, &fence_create_info, nullptr, &fence);
    return {cmd, fence};
  }
  void submit(VkQueue queue) {
    VkCommandBufferSubmitInfo command_buffer_info{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    VkSubmitInfo2 submit{
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos    = &command_buffer_info,
    };
    vkQueueSubmit2(queue, 1, &submit, fence);
  }

  void uninit(VkDevice device, VkCommandPool pool) {
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    vkDestroyFence(device, fence, nullptr);
  }
};

using CommandBufferToken = core::handle_t<struct _CommandBuffer, u32>;
using StagingBufferToken = core::handle_t<struct _StagingBuffer, u32>;
using MeshToken          = core::handle_t<struct _MeshToken, u32>;

struct Job {
  MeshToken mesh_token;
  StagingBufferToken staging_buffer_token;
  GpuMesh mesh;
  CommandBufferToken command_buffer_token;
};

struct RefCountedStagingBuffer {
  usize inflight;
  StagingBuffer buffer;
};

struct TextureCache;

class MeshLoader {
public:
  using OnPrimitiveLoaded = void (*)(void*, vk::Device& device, MeshToken, GpuMesh, bool mesh_fully_loaded);

  MeshToken queue_mesh(vk::Device& device, core::str8 src, TextureCache& texture_cache);
  void work(vk::Device& device, OnPrimitiveLoaded callback, void* userdata);

  MeshLoader(vk::Device& device);
  void uninit(subsystem::video& v);

private:
  struct MeshJobInfo {
    usize inflight    = 0;
    bool staging_done = false;
    core::Task* task;
  };
  VkCommandPool pool;
  core::vec<Job> jobs;
  core::handle_map<MeshJobInfo, MeshToken> mesh_job_infos{};
  core::handle_map<CommandBuffer, CommandBufferToken> command_buffers{};
  core::handle_map<RefCountedStagingBuffer, StagingBufferToken> inflight_staging_buffers{};
  core::array<StagingBuffer, 1> staging_buffers_storage{};
  core::vec<StagingBuffer> staging_buffers{core::clear, staging_buffers_storage.storage()};
  friend struct LoadMeshTask;
};

#endif // INCLUDE_APP_MESH_LOADER_H_
