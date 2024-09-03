#include "../memory.h"
#include <core/core.h>

#include <cerrno>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace core::enum_helpers;

#ifdef MEM_DEBUG
  #include <cstdio>
#endif

namespace os {
#ifndef MEM_USE_MALLOC

void* mem_allocate(void* ptr, usize size, MemAllocationFlags flags) {
  if (any(flags & MemAllocationFlags::Reserve)) {
    ASSERT(ptr == nullptr);
    MEM_DEBUG_STMT(printf("Memory: reserving %zu bytes\n", size));

    usize alloc_size = size;
    MEM_DEBUG_STMT(alloc_size += mem_page_size());
    ptr = mmap(nullptr, alloc_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (ptr == MAP_FAILED) {
      core::panic("%s", strerrordesc_np(errno));
    }

    ASAN_POISON_MEMORY_REGION(ptr, alloc_size);

    MEM_DEBUG_STMT(ptr = (void*)((char*)ptr + mem_page_size()));
  }

  if (any(flags & MemAllocationFlags::Commit)) {
    MEM_DEBUG_STMT(printf("Memory: commiting [%p-%p) \n", ptr, (u8*)ptr + size));
    if (mprotect(ptr, size, PROT_READ | PROT_WRITE) == -1) {
      core::panic("%s", strerrordesc_np(errno));
    }
    ASAN_UNPOISON_MEMORY_REGION(ptr, size);
  }

  return ptr;
}
void mem_deallocate(void* ptr, usize size, MemDeallocationFlags flags) {
  ASSERT(ptr != nullptr);

  if (any(flags & MemDeallocationFlags::Decommit)) {
    MEM_DEBUG_STMT(printf("Memory: decommiting [%p-%p) \n", ptr, (u8*)ptr + size));
    if (madvise(ptr, size, MADV_DONTNEED) == -1) {
      core::panic("%s", strerror(errno));
    }
    if (mprotect(ptr, size, PROT_NONE) == -1) {
      core::panic("%s", strerror(errno));
    }
    ASAN_POISON_MEMORY_REGION(ptr, size);
  }
  if (any(flags & MemDeallocationFlags::Release)) {
    MEM_DEBUG_STMT(printf("Memory: releasing [%p-%p) \n", ptr, (u8*)ptr + size));

    usize alloc_size = size;
    MEM_DEBUG_STMT(alloc_size += mem_page_size());
    MEM_DEBUG_STMT(ptr = (void*)((char*)ptr - mem_page_size()));
    if (munmap(ptr, alloc_size) == -1) {
      core::panic("%s", strerror(errno));
    }
    ASAN_POISON_MEMORY_REGION(ptr, alloc_size);
  }
  return;
}
#endif // MEM_USE_MALLOC

const usize page_size_ = [] {
  return (usize)sysconf(_SC_PAGESIZE);
}();
usize mem_page_size() {
  return page_size_;
}

} // namespace os
