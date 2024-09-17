#include "../memory.h"
#include <core/core.h>

#include <cerrno>
#include <string.h>
#include <windows.h>

using namespace core::enum_helpers;


namespace os {
#ifndef MEM_USE_MALLOC

void* mem_allocate(void* ptr, usize size, MemAllocationFlags flags) {
  DWORD fl {};
  if  (any(flags & MemAllocationFlags::Reserve)) {
    fl |= MEM_RESERVE;
  }
  if  (any(flags & MemAllocationFlags::Commit)) {
    fl |= MEM_COMMIT;
  }
  return VirtualAlloc(ptr, size, fl, PAGE_READWRITE);
}

void mem_deallocate(void* ptr, usize size, MemDeallocationFlags flags) {
  DWORD fl {};
  if  (any(flags & MemDeallocationFlags::Release)) {
    fl |= MEM_RELEASE;
    size = 0;
  }
  if  (any(flags & MemDeallocationFlags::Decommit)) {
    fl |= MEM_DECOMMIT;
  }
  VirtualFree(ptr, size, fl);
  return;
}
#endif // MEM_USE_MALLOC

const usize page_size_ = [] {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (usize)si.dwPageSize;
}();
usize mem_page_size() {
  return page_size_;
}

} // namespace os
