#ifdef VMA_DEBUG
  #include <core/core.h>
  #define VMA_DEBUG_LOG_FORMAT(format, ...) LOG_TRACE(format, __VA_ARGS__)
  #define VMA_ASSERT(expr) ASSERT(expr)
#endif

#define VMA_IMPLEMENTATION
#define VMA_CALL_PRE EXPORT
#include <vk_mem_alloc.h>
