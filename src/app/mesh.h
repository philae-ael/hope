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
    return {buffer, allocation, 0, size};
  }

  void copyMemoryToBuffer(
      vk::Device& device,
      VkCommandBuffer cmd,
      void* src,
      VmaAllocation dst_allocation,
      VkBuffer dst_buffer,
      VkDeviceSize dst_offset,
      VkDeviceSize dst_size
  ) {
    ASSERT(offset + dst_size < size);
    vmaCopyMemoryToAllocation(device.allocator, src, dst_allocation, offset, dst_size);

    VkBufferCopy region{
        .srcOffset = offset,
        .dstOffset = dst_offset,
        .size      = dst_size,
    };
    vkCmdCopyBuffer(cmd, buffer, dst_buffer, 1, &region);

    offset += dst_size;
  }

  void copyMemoryToImage(
      vk::Device& device,
      VkCommandBuffer cmd,
      void* src,
      VmaAllocation dst_allocation,
      VkImage dst_image,
      VkImageLayout dst_image_layout,
      VkExtent3D dst_image_extent3,
      usize texel_size,
      VkOffset3D dst_image_offset = {},
      u32 image_width             = 0,
      u32 image_height            = 0
  ) {
    usize dst_size = image_width * image_height * texel_size;
    ASSERT(offset + dst_size < size);
    vmaCopyMemoryToAllocation(device.allocator, src, dst_allocation, offset, size);

    VkBufferImageCopy region{
        .bufferOffset      = offset,
        .bufferRowLength   = image_width,
        .bufferImageHeight = image_height,
        .imageOffset       = dst_image_offset,
        .imageExtent       = dst_image_extent3,
    };

    vkCmdCopyBufferToImage(cmd, buffer, dst_image, dst_image_layout, 1, &region);

    offset += dst_size;
  }

  void copyMemoryToImage(
      vk::Device& device,
      VkCommandBuffer cmd,
      void* src,
      vk::image2D& image,
      usize texel_size,
      VkOffset3D dst_image_offset = {},
      u32 image_width             = 0,
      u32 image_height            = 0
  ) {
    copyMemoryToImage(
        device, cmd, src, image.allocation, image.image, image.sync.layout, image.extent.extent3, texel_size,
        dst_image_offset, image_width, image_height
    );
  }
  void uninit();
};

struct GpuMesh {
  VkBuffer vertex_buffer;
  VkBuffer index_buffer;
  VmaAllocation vertex_buf_allocation;
  VmaAllocation index_buf_allocation;
  vk::image2D base_color;

  math::Mat4 transform = math::Mat4::Id;
  u32 indice_count;
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
  using Callback = void (*)(void*, MeshToken, GpuMesh, bool mesh_fully_loaded);
  MeshToken queue_mesh(subsystem::video& v, core::str8 src);
  void work(Callback callback, void* userdata) {
    // Do work
    // Get staging token
    usize idx = jobs.size();
    for (auto& job : jobs.iter_rev()) {
      idx -= 1;
      if (true) {
        bool mesh_fully_loaded  = false;
        auto& infos             = mesh_job_infos[job.mesh_token].expect("counter does not exist!");
        infos.counter          -= 1;
        if (infos.counter == 0) {
          mesh_fully_loaded = true;
          mesh_job_infos.destroy(job.mesh_token);
        }

        callback(userdata, job.mesh_token, job.mesh, mesh_fully_loaded);
        jobs.swap_last_pop(idx);
      }
    }
  }

  bool loading() {
    return jobs.size() > 0;
  }

  static MeshLoader init() {}

private:
  VkCommandPool pool;
  struct MeshJobInfo {
    VkCommandBuffer buf;
    usize counter;
  };
  core::vec<Job> jobs;
  core::handle_map<MeshJobInfo, MeshToken> mesh_job_infos;
  core::handle_map<StagingBuffer, StagingBufferToken> staging_buffers;
};

#endif // INCLUDE_APP_MESH_LOADER_H_
