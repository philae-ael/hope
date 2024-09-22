#ifndef INCLUDE_VULKAN_INIT_H_
#define INCLUDE_VULKAN_INIT_H_

#include <vulkan/vulkan_core.h>

#include "vulkan.h"
#include <core/containers/vec.h>
#include <core/core/fwd.h>

namespace vk {
// GENERAL
void init();

// INSTANCE CREATION

struct Instance {
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;

  operator VkInstance() {
    return instance;
  }
};
Result<Instance> create_default_instance(core::vec<const char*> layers, core::vec<const char*> extensions);
void destroy_instance(Instance& inst);

core::vec<VkLayerProperties> enumerate_instance_layer_properties(core::Arena& ar);
core::vec<VkExtensionProperties> enumerate_instance_extension_properties(core::Arena& ar);

Result<VkInstance> create_instance(core::storage<const char*> layers, core::storage<const char*> extensions);
void load_extensions(VkInstance instance);
Result<VkDebugUtilsMessengerEXT> setup_debug_messenger(VkInstance instance);

struct Device {
  VkPhysicalDevice physical;
  VkDevice logical;

  VkPhysicalDeviceProperties properties;

  u32 omni_queue_family_index;
  VkQueue omni_queue;

  operator VkDevice() const {
    return logical;
  }
};
void destroy_device(vk::Device device);

core::vec<VkPhysicalDevice> enumerate_physical_devices(core::Arena& ar, VkInstance instance);

struct queue_request {
  VkQueueFlags flags;
  u32 count;
  VkSurfaceKHR surface_to_support = nullptr;
};

struct queue_creation_info {
  u32 queue_request_index;
  u32 family_index;
  u32 count;
};

struct physical_device_features {
  core::storage<queue_request> queues;
  bool synchronization2;
  bool dynamic_rendering;
  bool timestamps;

  bool check_features(const VkPhysicalDeviceProperties2& physical_device_properties2) const;
  VkPhysicalDeviceFeatures2 into_vk_physical_device_features2(core::Allocator alloc) const;
};

struct physical_device {
  VkPhysicalDevice physical_device;
  core::storage<queue_creation_info> queues_creation_infos;
};
core::Maybe<physical_device> find_physical_device(
    core::Arena& ar_,
    VkInstance instance,
    const physical_device_features& features
);

Result<Device> create_default_device(VkInstance instance, VkSurfaceKHR surface, core::vec<const char*> extensions);

struct SwapchainConfig {
  u32 min_image_count;
  union {
    VkExtent2D extent2;
    VkExtent3D extent3;
  };
  VkSurfaceFormatKHR surface_format;
  VkPresentModeKHR present_mode;
  VkImageUsageFlags image_usage_flags;
};

struct Swapchain {
  VkSwapchainKHR swapchain;
  SwapchainConfig config;
  core::storage<VkImage> images;

  operator VkSwapchainKHR() {
    return swapchain;
  }
};

core::vec<VkPresentModeKHR> enumerate_physical_device_surface_present_modes(
    core::Arena& ar,
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
);

core::vec<VkSurfaceFormatKHR> enumerate_physical_device_surface_formats(
    core::Arena& ar,
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
);

core::vec<VkImage> get_swapchain_images(core::Arena& ar, VkDevice device, VkSwapchainKHR swapchain);

Result<VkSwapchainKHR> create_swapchain(
    VkDevice device,
    core::storage<const u32> queue_family_indices,
    VkSurfaceKHR surface,
    const SwapchainConfig& swapchain_config,
    VkSwapchainKHR old_swapchain = VK_NULL_HANDLE
);

SwapchainConfig create_default_swapchain_config(const Device& device, VkSurfaceKHR surface);
Result<Swapchain> create_default_swapchain(core::Arena& ar, const Device& device, VkSurfaceKHR surface);

Result<core::unit_t> rebuild_default_swapchain(const Device& device, VkSurfaceKHR surface, Swapchain& swapchain);

void destroy_swapchain(VkDevice device, Swapchain& swapchain);
} // namespace vk

#endif // INCLUDE_VULKAN_INIT_H_
