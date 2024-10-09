#ifndef INCLUDE_VULKAN_DESCRIPTOR_H_
#define INCLUDE_VULKAN_DESCRIPTOR_H_

#include "vulkan.h"
#include <core/core.h>
#include <vulkan/vulkan_core.h>

namespace vk {
struct DescriptorPoolBuilder {
  u32 max_sets;
  core::storage<VkDescriptorPoolSize> sizes;

  VkDescriptorPool build(VkDevice device) {
    VkDescriptorPoolCreateInfo descriptor_pool_create_info{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = 1,
        .poolSizeCount = (u32)sizes.size,
        .pPoolSizes    = sizes.data,
    };
    VkDescriptorPool descriptor_pool;
    vkCreateDescriptorPool(device, &descriptor_pool_create_info, nullptr, &descriptor_pool);
    return descriptor_pool;
  }
};
struct DescriptorSetLayoutBuilder {
  core::storage<VkDescriptorSetLayoutBinding> bindings;
  VkDescriptorSetLayout build(VkDevice device) {
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (u32)bindings.size,
        .pBindings    = bindings.data,
    };
    VkDescriptorSetLayout descriptor_set_layout;
    vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout);
    return descriptor_set_layout;
  }
};

struct DescriptorSetAllocateInfo {
  VkDescriptorPool pool;
  core::storage<VkDescriptorSetLayout> layouts;
  VkDescriptorSetAllocateInfo vk() {
    return VkDescriptorSetAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = pool,
        .descriptorSetCount = (u32)layouts.size,
        .pSetLayouts        = layouts.data,
    };
  }
  VkDescriptorSet allocate(VkDevice device) {
    VkDescriptorSet descriptor_set;
    auto descriptor_set_alloc_infos = vk();
    vkAllocateDescriptorSets(device, &descriptor_set_alloc_infos, &descriptor_set);
    return descriptor_set;
  }
};

struct DescriptorSetWriter {
  VkDescriptorSet set;
  u32 dst_binding;
  u32 dst_array_element;
  VkDescriptorType descriptor_type;
  enum class Tag { Image, Buffer };
  union {
    Tag tag;
    struct {
      Tag tag = Tag::Image;
      core::storage<VkDescriptorImageInfo> image_infos;
    } image;
    struct {
      Tag tag = Tag::Buffer;
      core::storage<VkDescriptorBufferInfo> buffer_infos;
    } buffer;
  };

  VkWriteDescriptorSet vk() {
    VkWriteDescriptorSet w{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = set,
        .dstBinding      = dst_binding,
        .dstArrayElement = dst_array_element,
        .descriptorType  = descriptor_type,
    };
    switch (tag) {
    case Tag::Image:
      w.descriptorCount = (u32)image.image_infos.size;
      w.pImageInfo      = image.image_infos.data;
      break;
    case Tag::Buffer:
      w.descriptorCount = (u32)buffer.buffer_infos.size;
      w.pBufferInfo     = buffer.buffer_infos.data;
      break;
    }
    return w;
  }
};

struct DescriptorSetUpdater {
  core::storage<VkWriteDescriptorSet> write_descriptor_set{};
  core::storage<VkCopyDescriptorSet> copy_descriptor_set{};
  void update(VkDevice device) {
    vkUpdateDescriptorSets(
        device, (u32)write_descriptor_set.size, write_descriptor_set.data, (u32)copy_descriptor_set.size,
        copy_descriptor_set.data
    );
  }
};

} // namespace vk

#endif // INCLUDE_VULKAN_DESCRIPTOR_H_
