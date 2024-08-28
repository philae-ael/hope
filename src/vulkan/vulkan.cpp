
#include "vulkan.h"
#include "../core/core.h"

using namespace core::literals;

core::str8 to_str8(VkResult res) {
#define CASE(x) \
  case x:       \
    return STRINGIFY(x) ""_s;

  switch (res) {
    CASE(VK_SUCCESS)
    CASE(VK_NOT_READY)
    CASE(VK_TIMEOUT)
    CASE(VK_EVENT_SET)
    CASE(VK_EVENT_RESET)
    CASE(VK_INCOMPLETE)
    CASE(VK_ERROR_OUT_OF_HOST_MEMORY)
    CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
    CASE(VK_ERROR_INITIALIZATION_FAILED)
    CASE(VK_ERROR_DEVICE_LOST)
    CASE(VK_ERROR_MEMORY_MAP_FAILED)
    CASE(VK_ERROR_LAYER_NOT_PRESENT)
    CASE(VK_ERROR_EXTENSION_NOT_PRESENT)
    CASE(VK_ERROR_FEATURE_NOT_PRESENT)
    CASE(VK_ERROR_INCOMPATIBLE_DRIVER)
    CASE(VK_ERROR_TOO_MANY_OBJECTS)
    CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
    CASE(VK_ERROR_FRAGMENTED_POOL)
    CASE(VK_ERROR_UNKNOWN)
    CASE(VK_ERROR_OUT_OF_POOL_MEMORY)
    CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
    CASE(VK_ERROR_FRAGMENTATION)
    CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
    CASE(VK_PIPELINE_COMPILE_REQUIRED)
    CASE(VK_ERROR_SURFACE_LOST_KHR)
    CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
    CASE(VK_SUBOPTIMAL_KHR)
    CASE(VK_ERROR_OUT_OF_DATE_KHR)
    CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
    CASE(VK_ERROR_VALIDATION_FAILED_EXT)
    CASE(VK_ERROR_INVALID_SHADER_NV)
    CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)
    CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)
    CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)
    CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)
    CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)
    CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)
    CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
    CASE(VK_ERROR_NOT_PERMITTED_KHR)
    CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
    CASE(VK_THREAD_IDLE_KHR)
    CASE(VK_THREAD_DONE_KHR)
    CASE(VK_OPERATION_DEFERRED_KHR)
    CASE(VK_OPERATION_NOT_DEFERRED_KHR)
    CASE(VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR)
    CASE(VK_ERROR_COMPRESSION_EXHAUSTED_EXT)
    CASE(VK_INCOMPATIBLE_SHADER_BINARY_EXT)
  default:
    return "<unknown VkResult>"_s;
  }
#undef CASE
}
