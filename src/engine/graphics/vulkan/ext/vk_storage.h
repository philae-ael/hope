#ifndef INCLUDE_EXT_VK_STORAGE_H_
#define INCLUDE_EXT_VK_STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkStagingBufferCEXT)

typedef struct VkStagingRegionCEXT {
  size_t offset;
  VkDeviceSize dstOffset;
  VkDeviceSize size;
} VkStagingRegionCEXT;

typedef struct VkStagingBufferCreateInfoCEXT {
  VkStructureType sType;
  const void* pNext;
  VkBufferCreateFlags flags;
  VkDeviceSize size;
} VkStagingBufferCreateInfoCEXT;

typedef struct VkStagingBufferBudgetCEXT {
  VkDeviceSize usage;
  VkDeviceSize budget;
} VkStagingBufferBudgetCEXT;

VkResult VkCreateStagingBufferCEXT(
    VkDevice device,
    const VkStagingBufferCreateInfoCEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkStagingBufferCEXT* pStagingBuffer
);
VkResult VkDestroyStagingBufferCEXT(VkDevice device);
VkResult VkBindStagingBufferMemoryCEXT(
    VkDevice devivce,
    VkStagingBufferCEXT stagingBuffer,
    VkDeviceMemory memory,
    VkDeviceSize memoryOffset
);
void VkGetStagingBufferBudgetCEXT(VkStagingBufferCEXT stagingBuffer, VkStagingBufferBudgetCEXT* pBudget);

VkResult VkCmdStageMemoryCEXT(
    VkCommandBuffer commandBuffer,
    VkStagingBufferCEXT staging_buffer,
    void* pMemory,
    VkDeviceSize size,
    VkStagingRegionCEXT* pRegion
);

/// Note: This one can be implicitly async
// void VkCmdCopyFileToBufferCEXT(
//     VkCommandBuffer commandBuffer,
//     VkStagingBufferCEXT staging_buffer,
//     FILE* file,
//     VkBuffer dstBuffer,
//     uint32_t regionCount,
//     const VkFileBufferCopyCEXT* pRegions
// );

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_EXT_VK_STORAGE_H_
