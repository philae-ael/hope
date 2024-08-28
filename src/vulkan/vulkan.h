#ifndef INCLUDE_VULKAN_VULKAN_H_
#define INCLUDE_VULKAN_VULKAN_H_

#include "../core/core.h"

// IWYU pragma: begin_exports
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"
// IWYU pragma: end_exports

#define EXTENSIONS                      \
  LOAD(vkSetDebugUtilsObjectNameEXT)    \
  LOAD(vkSetDebugUtilsObjectTagEXT)     \
  LOAD(vkQueueBeginDebugUtilsLabelEXT)  \
  LOAD(vkQueueEndDebugUtilsLabelEXT)    \
  LOAD(vkQueueInsertDebugUtilsLabelEXT) \
  LOAD(vkCmdBeginDebugUtilsLabelEXT)    \
  LOAD(vkCmdEndDebugUtilsLabelEXT)      \
  LOAD(vkCmdInsertDebugUtilsLabelEXT)   \
  LOAD(vkCreateDebugUtilsMessengerEXT)  \
  LOAD(vkDestroyDebugUtilsMessengerEXT) \
  LOAD(vkSubmitDebugUtilsMessageEXT)

#define vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_
#define vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT_
#define vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT_
#define vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT_
#define vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_
#define vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_
#define vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_
#define vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT_
#define vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_
#define vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_
#define vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT_

#define EVAL(a) a
#define LOAD(name) extern PFN_##name name##_;
EVAL(EXTENSIONS)
#undef LOAD

namespace vk {
template <class T>
struct Result {
  enum class Discriminant : u8 { Err, Ok } d;
  union {
    VkResult res;
    T t;
  };

  Result(VkResult res) {
    d         = Discriminant::Err;
    this->res = res;
  }
  Result(T t) {
    d       = Discriminant::Ok;
    this->t = t;
  }

  inline bool is_ok() {
    return d == Discriminant::Ok;
  }
  inline bool is_err() {
    return d == Discriminant::Err;
  }
  inline VkResult err() {
    return res;
  }
  inline T& value() {
    return t;
  }
  inline T& operator*() {
    return value();
  }
  inline T* operator->() {
    return &t;
  }

  T expect(const char* msg) {
    if (is_err()) {
      LOG_BUILDER(
          core::LogLevel::Error, pushf("Result expected to be %s, got ", msg).push(res).panic()
      );
    }
    return t;
  }
};

template <class C>
struct wrapper {
  auto& operator*() {
    return static_cast<C>(this)->inner;
  }
};

} // namespace vk

#define VK_PUSH_NEXT(parent, child) \
  do {                              \
    child.pNext  = parent.pNext;    \
    parent.pNext = &child;          \
  } while (0)

core::str8 to_str8(VkResult res);

#endif // INCLUDE_VULKAN_VULKAN_H_