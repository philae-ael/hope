#ifndef INCLUDE_VULKAN_INIT_H_
#define INCLUDE_VULKAN_INIT_H_

#include "../containers/vec.h"
#include "../core/core.h"
#include "vulkan.h"

namespace vk {

struct instance {
  VkInstance inner;
  VkDebugUtilsMessengerEXT debug_messenger = nullptr;

  VkInstance operator*() {
    return inner;
  }
};

void init();

Result<instance> create_default_instance(
    core::arena& ar_, core::vec<const char*> layers, core::vec<const char*> extensions
);

Result<instance>
create_instance(core::storage<const char*> layers, core::storage<const char*> extensions);
void load_extensions(VkInstance instance);
VkResult setup_debug_messenger(instance& inst);

core::vec<VkLayerProperties> enumerate_instance_layer_properties(core::arena& ar);
core::vec<VkExtensionProperties> enumerate_instance_extension_properties(core::arena& ar);

void destroy_instance(instance& inst);

} // namespace vk

#endif // INCLUDE_VULKAN_INIT_H_
