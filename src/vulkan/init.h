#ifndef INCLUDE_VULKAN_INIT_H_
#define INCLUDE_VULKAN_INIT_H_

#include "../containers/vec.h"
#include "../core/core.h"
#include "vulkan.h"
#include <vulkan/vulkan_core.h>

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
Result<Instance> create_default_instance(
    core::Arena& arena,
    core::vec<const char*> layers,
    core::vec<const char*> extensions
);
void destroy_instance(Instance& inst);

core::vec<VkLayerProperties> enumerate_instance_layer_properties(core::Arena& ar);
core::vec<VkExtensionProperties> enumerate_instance_extension_properties(core::Arena& ar);

Result<VkInstance> create_instance(
    core::storage<const char*> layers,
    core::storage<const char*> extensions
);
void load_extensions(VkInstance instance);
Result<VkDebugUtilsMessengerEXT> setup_debug_messenger(VkInstance instance);

struct Device {
  VkPhysicalDevice physical;
  VkDevice logical;

  u32 omniQueueFamilyIndex;
  VkQueue omniQueue;

  operator VkDevice() {
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
  bool check_features(const VkPhysicalDeviceProperties2& physical_device_properties2) const;
  VkPhysicalDeviceFeatures2 into_vk_physical_device_features2() const;
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

Result<Device> create_default_device(
    core::Arena& ar_,
    VkInstance instance,
    VkSurfaceKHR surface,
    core::vec<const char*> extensions
);

} // namespace vk

#endif // INCLUDE_VULKAN_INIT_H_
