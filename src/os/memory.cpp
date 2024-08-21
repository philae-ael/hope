// USE IT AS A FALLBACK for detecting leaks using!
#ifdef MEM_USE_MALLOC
  #include "memory.h"
  #include "../core/core.h"

  #include <cstdio>
  #include <cstdlib>

namespace os {

using namespace core::enum_helpers;

void* mem_allocate(void* ptr, usize size, MemAllocationFlags flags) {
  void* ret = nullptr;
  if (any(flags & os::MemAllocationFlags::Reserve)) {
    ASSERT(ptr == nullptr);
    MEM_DEBUG_STMT(printf("Memory: reserving %zu bytes\n", size));
    ret = ::calloc(size, 1);
    ASAN_POISON_MEMORY_REGION(ptr, size);
    ASSERT(ret != nullptr);
  }

  if (any(flags & os::MemAllocationFlags::Commit)) {
    MEM_DEBUG_STMT(printf("Memory: commiting [%p-%p) \n", ptr, (u8*)ptr + size));
    ASAN_UNPOISON_MEMORY_REGION(ptr, size);
  }
  return ret;
}

void mem_deallocate(void* ptr, usize size, MemDeallocationFlags flags) {
  if (any(flags & MemDeallocationFlags::Decommit)) {
    MEM_DEBUG_STMT(printf("Memory: decommiting [%p-%p) \n", ptr, (u8*)ptr + size));
    ASAN_POISON_MEMORY_REGION(ptr, size);
  }
  if (any(flags & os::MemDeallocationFlags::Release)) {
    MEM_DEBUG_STMT(printf("Memory: releasing [%p-%p) \n", ptr, (u8*)ptr + size));
    ASAN_POISON_MEMORY_REGION(ptr, size);
    free(ptr);
  }
}

} // namespace os
#endif // MEM_USE_MALLOC
