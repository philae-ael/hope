// IWYU pragma: private
#ifndef INCLUDE_OS_MEMORY_H_
#define INCLUDE_OS_MEMORY_H_

#include <core/core/fwd.h>

#ifdef MEM_DEBUG
  #define MEM_DEBUG_STMT(f) f
#else
  #define MEM_DEBUG_STMT(f)
#endif
namespace os {

enum class MemAllocationFlags : u8 {
  Reserve = 0x1,
  Commit  = 0x2,
};
void* mem_allocate(void* ptr, usize size, MemAllocationFlags flags);

enum class MemDeallocationFlags {
  Release  = 0x1,
  Decommit = 0x2,
};
void mem_deallocate(void* ptr, usize size, MemDeallocationFlags flags);

usize mem_page_size();
} // namespace os

#endif // INCLUDE_OS_MEMORY_H_
