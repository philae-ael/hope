#ifndef INCLUDE_VULKAN_VULKAN_H_
#define INCLUDE_VULKAN_VULKAN_H_

#include <core/core/log.h>

// IWYU pragma: begin_exports
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
// IWYU pragma: end_exports

namespace core {
struct Arena;
} // namespace core

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

struct queue_flags_t {};
constexpr queue_flags_t queue_flags{};
core::str8 to_str8(core::Arena& ar, queue_flags_t, VkQueueFlags struct_type);
} // namespace vk

#define VK_PUSH_NEXT(parent, child)           \
  do {                                        \
    (child)->pNext  = (void*)(parent)->pNext; \
    (parent)->pNext = (child);                \
  } while (0)

#define VK_ASSERT(x)                                                                \
  do {                                                                              \
    VkResult res_assert_ = (x);                                                     \
    if (res_assert_ != VK_SUCCESS) {                                                \
      LOG_BUILDER(                                                                  \
          ::core::LogLevel::Error,                                                  \
          push("expected VK_SUCCESS, " STRINGIFY(x) " returned ").push(res_assert_) \
      );                                                                            \
      ::core::panic("VK_ASSERT failed");                                            \
    }                                                                               \
  } while (0)

core::str8 to_str8(VkResult res);
core::str8 to_str8(VkStructureType struct_type);
core::str8 to_str8(VkFormat format);

#endif // INCLUDE_VULKAN_VULKAN_H_
