#include "init.h"
#include "vulkan.h"

#include <core/containers/vec.h>
#include <core/core.h>
#include <cstring>
#include <vulkan/vulkan_core.h>

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
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
  core::LogLevel level = core::LogLevel::Debug;
  switch (messageSeverity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    level = core::LogLevel::Trace;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    level = core::LogLevel::Debug;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    level = core::LogLevel::Warning;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    level = core::LogLevel::Error;
    break;
  default:
    break;
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

  if (log_filter(level)) {
    core::log_builder(level, core::source_location{core::str8::from("VULKAN"), core::str8::from("VULKAN"), (u32)-1})
        .pushf(
            "vulkan  %s [%s:%d]: %s", message_type, pCallbackData->pMessageIdName, pCallbackData->messageIdNumber,
            pCallbackData->pMessage
        )
        .emit();
  }

  return VK_FALSE;
}

} // namespace

namespace vk {
EXPORT void init() {}

EXPORT void load_extensions(VkInstance instance) {
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

EXPORT Result<VkDebugUtilsMessengerEXT> setup_debug_messenger(VkInstance instance) {
  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
      .messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debug_utils_messenger_callback,
      .pUserData       = nullptr,
  };
  VkDebugUtilsMessengerEXT debug_messenger;
  VkResult err = vkCreateDebugUtilsMessengerEXT(instance, &debug_create_info, nullptr, &debug_messenger);

  if (err != VK_SUCCESS) {
    return err;
  }
  return debug_messenger;
}

Result<VkInstance> create_instance(core::storage<const char*> layers, core::storage<const char*> extensions) {
  VkInstance inst;
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
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
      .messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debug_utils_messenger_callback,
      .pUserData       = nullptr,
  };
  VK_PUSH_NEXT(&instance_create_info, &debug_create_info);

  VkResult res = vkCreateInstance(&instance_create_info, nullptr, &inst);
  if (res != VK_SUCCESS) {
    return res;
  }
  return inst;
}

EXPORT core::vec<VkLayerProperties> enumerate_instance_layer_properties(core::Arena& ar) {
  u32 layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  core::vec<VkLayerProperties> v{};
  v.set_capacity(ar, layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, v.data());
  v.set_size(layer_count);

  return v;
}
EXPORT core::vec<VkExtensionProperties> enumerate_instance_extension_properties(core::Arena& ar) {
  u32 layer_count;
  vkEnumerateInstanceExtensionProperties(nullptr, &layer_count, nullptr);

  core::vec<VkExtensionProperties> v{};
  v.set_capacity(ar, layer_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &layer_count, v.data());
  v.set_size(layer_count);

  return v;
}

EXPORT Result<Instance> create_default_instance(core::vec<const char*> layers, core::vec<const char*> extensions) {
  auto ar = core::scratch_get();
  defer { ar.retire(); };

  {
    const auto instance_layer_properties = enumerate_instance_layer_properties(*ar);
    for (auto instance_layer_property : instance_layer_properties.iter()) {
      LOG_INFO(
          "available instance layer: \"%s\" (%s)", instance_layer_property.layerName,
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
      ASSERTM(layer_found, "instance layer required but not found: %s", layer);
      LOG_INFO("instance layer found: %s", layer);
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
      LOG_INFO("available instance extension: \"%s\"", instance_extension_property.extensionName);
    }

    for (auto extension : extensions.iter()) {
      bool extension_found = false;
      for (auto instance_extension_property : instance_extension_properties.iter()) {
        if (strcmp(extension, instance_extension_property.extensionName) == 0) {
          extension_found = true;
          break;
        }
      }
      ASSERTM(extension_found, "instance extension required but not found: %s", extension);
      LOG_INFO("instance extension found: %s", extension);
    }
  }

  auto instance = create_instance(layers, extensions);
  if (instance.is_err()) {
    return instance.err();
  }
  load_extensions(instance.value());
  auto debug_messenger = setup_debug_messenger(instance.value());
  if (debug_messenger.is_err()) {
    return debug_messenger.err();
  }

  return {
      {
          *instance,
          *debug_messenger,
      },
  };
}

EXPORT void destroy_instance(Instance& inst) {
  vkDestroyDebugUtilsMessengerEXT(inst.instance, inst.debug_messenger, nullptr);
  vkDestroyInstance(inst.instance, nullptr);
}

EXPORT core::vec<VkPhysicalDevice> enumerate_physical_devices(core::Arena& ar, VkInstance instance) {
  core::vec<VkPhysicalDevice> v;
  u32 physical_device_count;

  vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
  v.set_capacity(ar, physical_device_count);
  vkEnumeratePhysicalDevices(instance, &physical_device_count, v.data());
  v.set_size(physical_device_count);

  return v;
}

EXPORT core::vec<VkQueueFamilyProperties> enumerate_physical_device_queue_family_properties(
    core::Arena& ar,
    VkPhysicalDevice physical_device
) {
  core::vec<VkQueueFamilyProperties> v;
  u32 queue_family_properties_count;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_properties_count, nullptr);
  v.set_capacity(ar, queue_family_properties_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_properties_count, v.data());
  v.set_size(queue_family_properties_count);
  return v;
}

EXPORT core::Maybe<core::vec<queue_creation_info>> find_physical_device_queue_allocation(
    core::Arena& ar,
    VkPhysicalDevice physical_device,
    const physical_device_features& required_features
) {
  auto queue_family_properties = enumerate_physical_device_queue_family_properties(ar, physical_device);

  auto queue_requests = core::vec{required_features.queues}.clone(ar);
  core::vec<queue_creation_info> queues;
  for (auto [family_index, queue_family_property] : core::enumerate{queue_family_properties.iter()}) {

    LOG_BUILDER(
        core::LogLevel::Debug,
        pushf("queue family %zu with count %u and flags ", family_index, queue_family_property->queueCount)
            .push(queue_flags, queue_family_property->queueFlags)
    );

    for (auto [request_index, queue_request] : core::enumerate{queue_requests.iter()}) {

      if (queue_family_property->queueCount == 0) {
        break;
      }
      if (queue_request->count == 0) {
        continue;
      }
      if (queue_request->surface_to_support != nullptr) {
        VkBool32 supported;
        VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device, (u32)family_index, queue_request->surface_to_support, &supported
        );
        ASSERT(res == VK_SUCCESS);
        if (supported != VK_TRUE) {
          LOG_DEBUG("presentation requested but not available in queue family with index %zu", family_index);
          continue;
        }
      }

      if ((queue_request->flags & queue_family_property->queueFlags) != 0) {
        u32 queue_count                    = MIN(queue_family_property->queueCount, queue_request->count);
        queue_family_property->queueCount -= queue_count;
        queue_request->count              -= queue_count;
        queues.push(
            ar,
            {
                .queue_request_index = (u32)request_index,
                .family_index        = (u32)family_index,
                .count               = queue_count,
            }
        );
      }
    }
  }
  for (auto [queue_request_index, queue_request] : core::enumerate{queue_requests.iter()}) {
    if (queue_request->count != 0) {
      LOG_DEBUG("queue request of index %zu not satisfied!", queue_request_index);
      return {};
    }
  }
  return queues;
}

EXPORT core::Maybe<physical_device> find_physical_device(
    core::Arena& ar,
    VkInstance instance,
    const physical_device_features& required_features
) {
  const auto physical_devices = enumerate_physical_devices(ar, instance);
  for (auto physical_device : physical_devices.iter()) {
    VkPhysicalDeviceProperties2 physical_device_properties2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };
    vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties2);

    if (!required_features.check_features(physical_device_properties2)) {
      LOG_DEBUG("physical device rejected: required features not satisfied");
      continue;
    }
    auto queue_creation_infos = find_physical_device_queue_allocation(ar, physical_device, required_features);
    if (queue_creation_infos.is_none()) {
      LOG_DEBUG("physical device rejected: can't allocate requested queues");
      continue;
    }

    return {{
        physical_device,
        queue_creation_infos.value(),
    }};
  }
  LOG_WARNING("no physical_device found");
  return {};
}

EXPORT core::vec<VkExtensionProperties> enumerate_device_extension_properties(
    core::Arena& ar,
    VkPhysicalDevice device
) {
  core::vec<VkExtensionProperties> v;
  u32 device_extension_count;

  vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, nullptr);
  v.set_capacity(ar, device_extension_count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, v.data());
  v.set_size(device_extension_count);

  return v;
}

EXPORT Result<VkDevice> create_device(
    core::Arena& ar_,
    VkPhysicalDevice physical_device,
    physical_device_features& features,
    core::storage<const char*> extensions,
    core::storage<queue_creation_info> queue_create_infos
) {
  core::ArenaTemp ar = ar_.make_temp();
  defer { ar.retire(); };
  core::Allocator alloc = *ar;

  core::vec<VkDeviceQueueCreateInfo> queue_device_create_infos;
  for (auto queue_creation_info : queue_create_infos.iter()) {
    auto priorities = alloc.allocate_array<f32>(queue_creation_info.count);
    for (auto i : core::range{0zu, (usize)queue_creation_info.count}.iter()) {
      priorities[i] = 1.0;
    }
    queue_device_create_infos.push(
        *ar,
        {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_creation_info.family_index,
            .queueCount       = 1,
            .pQueuePriorities = priorities.data,
        }
    );
  }

  VkPhysicalDeviceFeatures2 requested_features2 = features.into_vk_physical_device_features2(*ar);
  VkDeviceCreateInfo device_create_info{
      .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext                   = &requested_features2,
      .queueCreateInfoCount    = (u32)queue_device_create_infos.size(),
      .pQueueCreateInfos       = queue_device_create_infos.data(),
      .enabledExtensionCount   = (u32)extensions.size,
      .ppEnabledExtensionNames = extensions.data,
  };
  VkDevice device;
  VkResult res = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
  if (res != VK_SUCCESS) {
    return res;
  }
  return device;
}

EXPORT Result<Device> create_default_device(
    VkInstance instance,
    VkSurfaceKHR surface,
    core::vec<const char*> device_extensions
) {
  auto ar = core::scratch_get();
  defer { ar.retire(); };

  queue_request queues[]{
      {
          .flags              = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,
          .count              = 1,
          .surface_to_support = surface,
      },
  };
  physical_device_features requested_features{
      .queues              = queues,
      .synchronization2    = true,
      .dynamic_rendering   = true,
      .timestamps          = true,
      .descriptor_indexing = true,
  };
  auto [physical_device, queues_creation_infos] = [&] {
    auto physical_device = find_physical_device(*ar, instance, requested_features);
    ASSERTM(physical_device.is_some(), "no physical device found");
    return *physical_device;
  }();

  ASSERT(queues_creation_infos.size == 1);

  device_extensions.push(*ar, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  device_extensions.push(*ar, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
  auto device_extension_properties = enumerate_device_extension_properties(*ar, physical_device);
  for (auto& extension : device_extensions.iter()) {
    bool found = false;
    for (auto& device_extension_property : device_extension_properties.iter()) {
      if (strcmp(extension, device_extension_property.extensionName) == 0) {
        found = true;
      }
    }
    ASSERTM(found, "device extension required but not found: %s", extension);
    LOG_INFO("device extension found: %s", extension);
  }
  auto device = create_device(*ar, physical_device, requested_features, device_extensions, queues_creation_infos);
  if (device.is_err()) {
    return device.err();
  }

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_device, &properties);

  VkQueue omniQueue;
  u32 omniQueueFamilyIndex = queues_creation_infos[0].family_index;
  vkGetDeviceQueue(*device, omniQueueFamilyIndex, 0, &omniQueue);
  return Device{
      physical_device, device.value(), properties, omniQueueFamilyIndex, omniQueue,

  };
}

EXPORT VkPhysicalDeviceFeatures2 physical_device_features::into_vk_physical_device_features2(core::Allocator alloc
) const {
  VkPhysicalDeviceFeatures2 physical_device_features2{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
  };
  if (synchronization2) {
    auto physical_device_synchronization2_features = alloc.allocate<VkPhysicalDeviceSynchronization2Features>();
    *physical_device_synchronization2_features     = {
            .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .synchronization2 = VK_TRUE,
    };
    VK_PUSH_NEXT(&physical_device_features2, physical_device_synchronization2_features);
  }
  if (dynamic_rendering) {
    auto physical_device_dynamic_rendering_features = alloc.allocate<VkPhysicalDeviceDynamicRenderingFeatures>();
    *physical_device_dynamic_rendering_features     = {
            .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .dynamicRendering = true,
    };
    VK_PUSH_NEXT(&physical_device_features2, physical_device_dynamic_rendering_features);
  }

  if (timestamps) {
    auto physical_device_host_query_reset_features = alloc.allocate<VkPhysicalDeviceHostQueryResetFeaturesEXT>();
    *physical_device_host_query_reset_features     = {
            .sType          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT,
            .hostQueryReset = VK_TRUE,
    };
    VK_PUSH_NEXT(&physical_device_features2, physical_device_host_query_reset_features);
  }
  if (descriptor_indexing) {
    auto physical_device_descriptor_indexing_features = alloc.allocate<VkPhysicalDeviceDescriptorIndexingFeatures>();
    *physical_device_descriptor_indexing_features     = {
            .sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
            .runtimeDescriptorArray                    = VK_TRUE,
    };
    VK_PUSH_NEXT(&physical_device_features2, physical_device_descriptor_indexing_features);
  }
  return physical_device_features2;
}
EXPORT bool physical_device_features::check_features(const VkPhysicalDeviceProperties2& physical_device_properties2
) const {
  auto s = (const VkBaseInStructure*)physical_device_properties2.pNext;
  while (s != nullptr) {
    switch (s->sType) {
    default:
      LOG_BUILDER(core::LogLevel::Error, pushf("Feature not supported in check_features").push(s->sType));
    }
    s = s->pNext;
  }
  return true;
}

EXPORT void destroy_device(vk::Device device) {
  vkDestroyDevice(device, nullptr);
}
EXPORT core::vec<VkPresentModeKHR> enumerate_physical_device_surface_present_modes(
    core::Arena& ar,
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
) {
  core::vec<VkPresentModeKHR> v;
  u32 present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
  v.set_capacity(ar, present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, v.data());
  v.set_size(present_mode_count);

  return v;
}

EXPORT core::vec<VkSurfaceFormatKHR> enumerate_physical_device_surface_formats(
    core::Arena& ar,
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
) {
  core::vec<VkSurfaceFormatKHR> v;
  u32 surface_format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, nullptr);
  v.set_capacity(ar, surface_format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, v.data());
  v.set_size(surface_format_count);

  return v;
}

EXPORT core::vec<VkImage> get_swapchain_images(core::Allocator alloc, VkDevice device, VkSwapchainKHR swapchain) {
  core::vec<VkImage> v;
  u32 swapchain_image_count;
  vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
  v.set_capacity(alloc, swapchain_image_count);
  vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, v.data());
  v.set_size(swapchain_image_count);

  return v;
}

EXPORT void destroy_swapchain(VkDevice device, Swapchain& swapchain) {
  vkDestroySwapchainKHR(device, swapchain, nullptr);
}
Result<VkSwapchainKHR> create_swapchain(
    VkDevice device,
    core::storage<const u32> queue_family_indices,
    VkSurfaceKHR surface,
    const SwapchainConfig& swapchain_config,
    VkSwapchainKHR old_swapchain
) {
  VkSwapchainCreateInfoKHR swapchain_create_info{
      .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface               = surface,
      .minImageCount         = swapchain_config.min_image_count,
      .imageFormat           = swapchain_config.surface_format.format,
      .imageColorSpace       = swapchain_config.surface_format.colorSpace,
      .imageExtent           = swapchain_config.extent2,
      .imageArrayLayers      = 1,
      .imageUsage            = swapchain_config.image_usage_flags,
      .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = (u32)queue_family_indices.size,
      .pQueueFamilyIndices   = queue_family_indices.data,
      .preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode           = swapchain_config.present_mode,
      .clipped               = VK_TRUE,
      .oldSwapchain          = old_swapchain,
  };

  VkSwapchainKHR swapchain;
  VkResult res = vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain);
  if (res != VK_SUCCESS) {
    return res;
  }
  return swapchain;
}

EXPORT SwapchainConfig create_default_swapchain_config(const Device& device, VkSurfaceKHR surface) {
  auto ar = core::scratch_get();
  defer { ar.retire(); };
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical, surface, &surface_capabilities));

  SwapchainConfig swapchain_config{};
  swapchain_config.extent2       = surface_capabilities.currentExtent.width == 0xFFFFFFFF
                                       ? surface_capabilities.maxImageExtent
                                       : surface_capabilities.currentExtent;
  swapchain_config.extent3.depth = 1;
  swapchain_config.min_image_count =
      MAX(surface_capabilities.minImageCount,
          surface_capabilities.maxImageCount == 0 ? 3 : MIN(3, surface_capabilities.maxImageCount));

  {
    auto present_modes = enumerate_physical_device_surface_present_modes(*ar, device.physical, surface);
    ASSERT(present_modes.size() > 0);
    VkPresentModeKHR best_present_mode;
    int best_score = -1;

    for (auto present_mode : present_modes.iter()) {
      int score = 0;
      LOG_BUILDER(core::LogLevel::Debug, push("Surface accept present mode ").push(present_mode));
      switch (present_mode) {
      case VK_PRESENT_MODE_IMMEDIATE_KHR:
        score = 1;
        break;
      case VK_PRESENT_MODE_MAILBOX_KHR:
        score = 2;
        break;
      case VK_PRESENT_MODE_FIFO_KHR:
        score = 3;
        break;
      case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        score = 4;
        break;
      default:
        break;
      }
      if (score > best_score) {
        best_present_mode = present_mode;
        best_score        = score;
      }
    }

    swapchain_config.present_mode = best_present_mode;
    LOG_BUILDER(
        core::LogLevel::Info, push("Present Mode ").push(best_present_mode).pushf(" has been chosen for surface")
    );
  }

  {
    auto formats = enumerate_physical_device_surface_formats(*ar, device.physical, surface);
    ASSERT(formats.size() > 0);
    VkSurfaceFormatKHR best_format;
    int best_score = -1;

    for (auto format : formats.iter()) {
      LOG_BUILDER(core::LogLevel::Info, push("Surface accept format ").push(format.format));
      int score = 0;
      switch (format.format) {
      case VK_FORMAT_B8G8R8A8_SRGB:
      case VK_FORMAT_R8G8B8A8_SRGB:
        score = 1;
        break;
      default:
        break;
      }

      if (score > best_score) {
        best_format = format;
        best_score  = score;
      }
    }
    swapchain_config.surface_format = best_format;
    LOG_BUILDER(core::LogLevel::Info, push("Format ").push(best_format.format).pushf(" has been chosen for surface"));
  }
  VkFormatProperties surface_format_properties{};
  vkGetPhysicalDeviceFormatProperties(
      device.physical, swapchain_config.surface_format.format, &surface_format_properties
  );

  VkImageUsageFlags format_supported_usage_flags{};
  if (surface_format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
    format_supported_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (surface_format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
    format_supported_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  if (surface_format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
    format_supported_usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (surface_format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
    format_supported_usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (surface_format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
    format_supported_usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  swapchain_config.image_usage_flags = surface_capabilities.supportedUsageFlags & format_supported_usage_flags;
  ASSERT((swapchain_config.image_usage_flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0);
  return swapchain_config;
}

EXPORT Result<Swapchain> create_default_swapchain(core::Allocator alloc, const Device& device, VkSurfaceKHR surface) {
  auto swapchain_config = create_default_swapchain_config(device, surface);
  u32 queue_families[]{device.omni_queue_family_index};
  auto swapchain = create_swapchain(device, queue_families, surface, swapchain_config);
  return {{
      *swapchain,
      swapchain_config,
      get_swapchain_images(alloc, device, *swapchain),
  }};
}
EXPORT Result<core::unit_t> rebuild_default_swapchain(
    const Device& device,
    VkSurfaceKHR surface,
    Swapchain& swapchain
) {
  auto new_swapchain_config = create_default_swapchain_config(device, surface);
  ASSERTM(new_swapchain_config.min_image_count == swapchain.config.min_image_count, "min image count has changed...");
  u32 queue_families[]{device.omni_queue_family_index};
  auto new_swapchain = create_swapchain(device, queue_families, surface, new_swapchain_config, swapchain);
  if (new_swapchain.is_err()) {
    return new_swapchain.err();
  }

  vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
  swapchain.swapchain = new_swapchain.value();
  swapchain.config    = new_swapchain_config;
  vkGetSwapchainImagesKHR(device, swapchain, &swapchain.config.min_image_count, swapchain.images.data);
  return core::unit;
}
} // namespace vk
