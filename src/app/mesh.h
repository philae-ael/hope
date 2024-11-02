#ifndef INCLUDE_APP_MESH_LOADER_H_
#define INCLUDE_APP_MESH_LOADER_H_

#include "core/core.h"
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
  VkEvent event;
  VkDeviceSize offset;
  VkDeviceSize size;

  static StagingBuffer init(vk::Device& device, VkDeviceSize size) {
    VkBufferCreateInfo buffer_create_info{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size                  = size,
        .usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &device.omni_queue_family_index,
    };
    VmaAllocationCreateInfo allocation_create_info{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    VkBuffer buffer;
    VmaAllocation allocation;
    vmaCreateBuffer(device.allocator, &buffer_create_info, &allocation_create_info, &buffer, &allocation, nullptr);
    vmaSetAllocationName(device.allocator, allocation, "staging buffer");

    VkEventCreateInfo fence_create_info{
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
    };
    VkEvent event;
    vkCreateEvent(device, &fence_create_info, nullptr, &event);
    return {buffer, allocation, event, 0, size};
  }

  void copyMemoryToBuffer(
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
    return offset_ + dst_size < size;
  }

  void copyMemoryToImage(
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

  void copyMemoryToImage(
      vk::Device& device,
      VkCommandBuffer cmd,
      void* src,
      vk::image2D& dst,
      usize texel_size,
      VkOffset3D dst_image_offset,
      u32 image_width,
      u32 image_height
  ) {
    copyMemoryToImage(
        device, cmd, src, dst.image, dst.sync.layout, dst.extent.extent3, texel_size, dst_image_offset, image_width,
        image_height
    );
  }

  // Call this one after the data has been fully queued to upload
  void close(VkCommandBuffer cmd) {
    vkCmdSetEvent(cmd, event, VK_PIPELINE_STAGE_TRANSFER_BIT);
  }

  void uninit(vk::Device& device) {
    vmaDestroyBuffer(device.allocator, buffer, allocation);
    vkDestroyEvent(device, event, nullptr);
  }
  bool done(VkDevice device) {
    return vkGetEventStatus(device, event) == VK_EVENT_SET;
  }
};

struct Uploader {
  StagingBuffer staging_buffer;
};

struct GpuMesh {
  VkBuffer vertex_buffer;
  VkBuffer index_buffer;
  VmaAllocation vertex_buf_allocation;
  VmaAllocation index_buf_allocation;
  vk::image2D base_color;

  math::Mat4 transform = math::Mat4::Id;
  u32 indice_count;

  // if true, indices are u32
  bool huge_indices;
};

void unload_mesh(subsystem::video&, GpuMesh);

struct _StagingBufferToken {};
using StagingBufferToken = core::handle_t<_StagingBufferToken, u32>;

struct _MeshToken {};
using MeshToken = core::handle_t<_MeshToken, u32>;

struct Job {
  MeshToken mesh_token;
  StagingBufferToken staging_token;
  GpuMesh mesh;
};

// TODO: this is a coroutine... I should dive into c++ coroutines... I guess... maybe...
class MeshLoader {
public:
  using Callback = void (*)(void*, vk::Device& device, MeshToken, GpuMesh, bool mesh_fully_loaded);
  MeshToken queue_mesh(vk::Device& device, VkCommandBuffer cmd, core::str8 src);
  void work(vk::Device& device, Callback callback, void* userdata) {
    // Do work
    for (auto t : staging_buffers.iter_rev_enumerate()) {
      StagingBufferToken staging_token = *core::get<0>(t);
      StagingBuffer& buffer            = *core::get<1>(t);

      if (!buffer.done(device)) {
        continue;
      }

      usize idx = jobs.size();
      for (auto& job : jobs.iter_rev()) {
        idx -= 1;
        if (job.staging_token == staging_token) {
          bool mesh_fully_loaded  = false;
          auto& infos             = mesh_job_infos[job.mesh_token].expect("counter does not exist!");
          infos.counter          -= 1;
          if (infos.counter == 0) {
            mesh_fully_loaded = true;
            mesh_job_infos.destroy(job.mesh_token);
          }

          callback(userdata, device, job.mesh_token, job.mesh, mesh_fully_loaded);
          jobs.swap_last_pop(idx);
        }
      }

      buffer.uninit(device);
      staging_buffers.destroy(staging_token);
    }
  }

  bool loading() {
    return jobs.size() > 0;
  }

  static MeshLoader init(vk::Device& device) {
    return {};
  }

private:
  struct MeshJobInfo {
    VkCommandBuffer buf;
    usize counter;
  };
  core::vec<Job> jobs;
  core::handle_map<MeshJobInfo, MeshToken> mesh_job_infos;
  core::handle_map<StagingBuffer, StagingBufferToken> staging_buffers;
};

#endif // INCLUDE_APP_MESH_LOADER_H_
