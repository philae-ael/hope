#ifdef VMA_DEBUG
  #include <core/core.h>
  // #define VMA_DEBUG_LOG_FORMAT(format, ...) LOG_TRACE(format, __VA_ARGS__)
  #define VMA_ASSERT(expr) ASSERT(expr)
  #define VMA_CALL_PRE EXPORT
  #define VMA_LEAK_LOG_FORMAT(format, ...) LOG_ERROR((format), __VA_ARGS__)
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
