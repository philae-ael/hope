#include "../memory.h"
#include "../../prelude.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

namespace os {

#ifndef MEM_USE_MALLOC

MEM_DEBUG_STMT(std::atomic<usize> alloc_count = 0);
MEM_DEBUG_STMT(auto check_alloc_count = core ::defer_builder_t{} + [] {
  ASSERTM(
      alloc_count == 0,
      "A total of %zu bytes has been a leaked! (although a big part can be "
      "not provisioned)",
      alloc_count.load()
  );
});

void* mem_allocate(void* ptr, usize size, MemAllocationFlags flags) {
  if (any(flags & MemAllocationFlags::Reserve)) {
    ASSERT(ptr == nullptr);
    MEM_DEBUG_STMT(printf("Memory: reserving %zu bytes\n", size));

    usize alloc_size = size;
    MEM_DEBUG_STMT(alloc_size += mem_page_size());
    ptr = mmap(nullptr, alloc_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (ptr == MAP_FAILED) {
      panic("%s", std::source_location::current(), strerrordesc_np(errno));
    }

    ASAN_POISON_MEMORY_REGION(ptr, alloc_size);

    MEM_DEBUG_STMT(ptr = (void*)((char*)ptr + mem_page_size()));
    MEM_DEBUG_STMT(alloc_count += alloc_size);
  }

  if (any(flags & MemAllocationFlags::Commit)) {
    MEM_DEBUG_STMT(printf("Memory: commiting [%p-%p) \n", ptr, (u8*)ptr + size));
    if (mprotect(ptr, size, PROT_READ | PROT_WRITE) == -1) {
      panic("%s", std::source_location::current(), strerrordesc_np(errno));
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
      panic("%s", std::source_location::current(), strerror(errno));
    }
    if (mprotect(ptr, size, PROT_NONE) == -1) {
      panic("%s", std::source_location::current(), strerror(errno));
    }
    ASAN_POISON_MEMORY_REGION(ptr, size);
  }
  if (any(flags & MemDeallocationFlags::Release)) {
    MEM_DEBUG_STMT(printf("Memory: releasing [%p-%p) \n", ptr, (u8*)ptr + size));

    usize alloc_size = size;
    MEM_DEBUG_STMT(alloc_size += mem_page_size());
    MEM_DEBUG_STMT(ptr = (void*)((char*)ptr - mem_page_size()));
    if (munmap(ptr, alloc_size) == -1) {
      panic("%s", std::source_location::current(), strerror(errno));
    }
    ASAN_POISON_MEMORY_REGION(ptr, alloc_size);
    MEM_DEBUG_STMT(alloc_count -= alloc_size);
  }
  return;
}

const usize page_size_ = [] {
  return sysconf(_SC_PAGESIZE);
}();
usize mem_page_size() {
  return page_size_;
}

#endif // MEM_USE_MALLOC
} // namespace os
