#ifdef VMA_DEBUG
  #include <core/core.h>
  #define VMA_DEBUG_LOG_FORMAT(format, ...) LOG_DEBUG(format, __VA_ARGS__)
#endif

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
