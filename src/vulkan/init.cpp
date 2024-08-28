#include "init.h"
#include "vulkan.h"

#include "../containers/vec.h"
#include "../core/core.h"
#include <cstring>

namespace {
void notloaded() {
  core::panic("extension has not been loaded yet");
}
void cantload() {
  core::panic("extension that has not been successfully loaded is called");
}
} // namespace

#define LOAD(name) PFN_##name name##_ = (PFN_##name)notloaded;
EVAL(EXTENSIONS)
#undef LOAD

namespace {
VkBool32 debug_utils_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
) {
  core::LogLevel level = core::LogLevel::Debug;
  switch (messageSeverity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    level = core::LogLevel::Trace;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    level = core::LogLevel::Info;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    level = core::LogLevel::Warning;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    level = core::LogLevel::Error;
    break;
  default:
  }

  const char* message_type;
  if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
    message_type = "general";
  }
  if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
    message_type = "validation";
  }
  if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
    message_type = "performance";
  }

  LOG_BUILDER(
      level, pushf(
                 "vulkan  %s [%s:%d]: %s", message_type, pCallbackData->pMessageIdName,
                 pCallbackData->messageIdNumber, pCallbackData->pMessage
             )
  );
  return VK_FALSE;
}

} // namespace

namespace vk {
void init() {}

void load_extensions(VkInstance instance) {
#define LOAD(name)                                                   \
  name##_ = (PFN_##name)[&] {                                        \
    auto res = vkGetInstanceProcAddr(instance, #name);               \
    if (res != nullptr) {                                            \
      return res;                                                    \
    } else {                                                         \
      LOG_ERROR("loading of extension function `" #name "` failed"); \
      return cantload;                                               \
    }                                                                \
  }                                                                  \
  ();

  EVAL(EXTENSIONS)
#undef LOAD
}

VkResult setup_debug_messenger(instance& inst) {
  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{
      .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext           = nullptr,
      .flags           = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debug_utils_messenger_callback,
      .pUserData       = nullptr,
  };
  return vkCreateDebugUtilsMessengerEXT(*inst, &debug_create_info, nullptr, &inst.debug_messenger);
}

Result<instance>
create_instance(core::storage<const char*> layers, core::storage<const char*> extensions) {
  instance inst;
  VkApplicationInfo app_info{
      .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext              = nullptr,
      .pApplicationName   = "app",
      .applicationVersion = 0,
      .pEngineName        = nullptr,
      .engineVersion      = 0,
      .apiVersion         = VK_MAKE_API_VERSION(0, 1, 3, 0),
  };
  VkInstanceCreateInfo instance_create_info{
      .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext                   = nullptr,
      .flags                   = 0,
      .pApplicationInfo        = &app_info,
      .enabledLayerCount       = (u32)layers.size,
      .ppEnabledLayerNames     = layers.data,
      .enabledExtensionCount   = (u32)extensions.size,
      .ppEnabledExtensionNames = extensions.data,
  };
  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{
      .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext           = nullptr,
      .flags           = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debug_utils_messenger_callback,
      .pUserData       = nullptr,
  };
  VK_PUSH_NEXT(instance_create_info, debug_create_info);

  VkResult res = vkCreateInstance(&instance_create_info, nullptr, &inst.inner);
  if (res != VK_SUCCESS) {
    return res;
  }
  return inst;
}

core::vec<VkLayerProperties> enumerate_instance_layer_properties(core::arena& ar) {
  u32 layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  core::vec<VkLayerProperties> v{};
  v.set_capacity(ar, layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, v.data());
  v.set_size(layer_count);

  return v;
}
core::vec<VkExtensionProperties> enumerate_instance_extension_properties(core::arena& ar) {
  u32 layer_count;
  vkEnumerateInstanceExtensionProperties(nullptr, &layer_count, nullptr);

  core::vec<VkExtensionProperties> v{};
  v.set_capacity(ar, layer_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &layer_count, v.data());
  v.set_size(layer_count);

  return v;
}

Result<instance> create_default_instance(
    core::arena& ar_, core::vec<const char*> layers, core::vec<const char*> extensions
) {
  auto ar = ar_.make_temp();
  defer { ar.retire(); };

  {
    const auto instance_layer_properties = enumerate_instance_layer_properties(*ar);
    for (auto instance_layer_property : instance_layer_properties.iter()) {
      LOG_INFO(
          "available layer: \"%s\" (%s)", instance_layer_property.layerName,
          instance_layer_property.description
      );
    }

    for (auto layer : layers.iter()) {
      bool layer_found = false;
      for (auto instance_layer_property : instance_layer_properties.iter()) {
        if (strcmp(layer, instance_layer_property.layerName) == 0) {
          layer_found = true;
          break;
        }
      }
      ASSERTM(layer_found, "layer required but not found: %s", layer);
      LOG_INFO("layer found: %s", layer);
    }
  }

  {
    bool has_debug_util_ext = false;
    for (auto extension : extensions.iter()) {
      if (strcmp(extension, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
        has_debug_util_ext = true;
      }
    }
    if (!has_debug_util_ext) {
      extensions.push(*ar, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
  }
  {
    const auto instance_extension_properties = enumerate_instance_extension_properties(*ar);
    for (auto instance_extension_property : instance_extension_properties.iter()) {
      LOG_INFO("available extension: \"%s\"", instance_extension_property.extensionName);
    }

    for (auto extension : extensions.iter()) {
      bool extension_found = false;
      for (auto instance_extension_property : instance_extension_properties.iter()) {
        if (strcmp(extension, instance_extension_property.extensionName) == 0) {
          extension_found = true;
          break;
        }
      }
      ASSERTM(extension_found, "extension required but not found: %s", extension);
      LOG_INFO("extension found: %s", extension);
    }
  }

  auto instance = create_instance(layers, extensions);
  if (instance.is_err()) {
    return instance;
  }
  load_extensions(*instance.value());
  VkResult res = setup_debug_messenger(*instance);
  if (res != VK_SUCCESS) {
    return res;
  }

  return instance;
}

void destroy_instance(instance& inst) {
  vkDestroyDebugUtilsMessengerEXT(*inst, inst.debug_messenger, nullptr);
  vkDestroyInstance(*inst, nullptr);
}

} // namespace vk
