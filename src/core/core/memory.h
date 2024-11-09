// IWYU pragma: privatememh
#ifndef INCLUDE_CORE_MEMORY_H_
#define INCLUDE_CORE_MEMORY_H_

#include "debug.h"
#include "fwd.h"
#include "type_info.h"
#include "types.h"
#include <cstdlib>

#ifndef SCRATCH_ARENA_AMOUNT
  #define SCRATCH_ARENA_AMOUNT 6
#endif

#ifndef DEFAULT_ARENA_CAPACITY
  #define DEFAULT_ARENA_CAPACITY GB(1) // it's virtual memory, we have plenty
#endif

#ifndef ALLOC_DEFAULT_ALIGNEMENT
  #define ALLOC_DEFAULT_ALIGNEMENT max_align
#endif

// 0 for system value
// anything else will be used as page size
// WARNING ! Should be a multiple of system value (often 4kb)
#ifndef ARENA_PAGE_SIZE
  #define ARENA_PAGE_SIZE 0
#endif // !ARENA_PAGE_SIZE

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
  #include <sanitizer/asan_interface.h>

  #define ASAN_POISON_MEMORY_REGION(addr, size) __asan_poison_memory_region((addr), (size))
  #define ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
  #define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
  #define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

namespace core {

struct AllocatorVTable {
  using allocate_pfn = void* (*)(void*, usize size, usize alignement, const char* src);

  // if size == 0, it should be best effort
  using deallocate_pfn = void (*)(void*, void* alloc_base_ptr, usize size, const char* src);
  using try_resize_pfn = bool (*)(void*, void* alloc_base_ptr, usize cur_size, usize new_size, const char* src);
  using owns_pfn       = bool (*)(void*, void* alloc_base_ptr);

  allocate_pfn allocate;
  deallocate_pfn deallocate;
  try_resize_pfn try_resize;
  owns_pfn owns;
};

struct Allocator {
  void* userdata;
  const AllocatorVTable* vtable;

  inline void* allocate(usize size, usize alignement = ALLOC_DEFAULT_ALIGNEMENT, const char* src = "<unknown>") {
    return vtable->allocate(userdata, size, alignement, src);
  }
  inline void* allocate(LayoutInfo layout, const char* src = "<unknown>") {
    return vtable->allocate(userdata, layout.size, layout.alignement, src);
  }

  template <class T>
  inline T* allocate() {
    return static_cast<T*>(allocate(default_layout_of<T>(), type_name<T>()));
  }
  storage<u8> allocate_array(LayoutInfo layout, usize element_count, const char* src = "<unknown>") {
    auto arr_layout = layout.array(element_count);
    return {
        arr_layout.size,
        (u8*)allocate(arr_layout, src),
    };
  }
  template <class T>
  storage<T> allocate_array(usize element_count, const char* src = type_name<T>()) {
    auto arr_layout = default_layout_of<T>().array(element_count);
    return {
        element_count,
        (T*)allocate(arr_layout, src),
    };
  }

  inline void deallocate(void* alloc_base_ptr, usize size, const char* src = "<unknown>") {
    return vtable->deallocate(userdata, alloc_base_ptr, size, src);
  }
  template <class T>
  inline void deallocate(T* alloc_base_ptr, const char* src = "<unknown>") {
    return vtable->deallocate(userdata, (void*)alloc_base_ptr, sizeof(T), src);
  }

  inline bool try_resize(void* alloc_base_ptr, usize cur_size, usize new_size, const char* src = "<unknown>") {
    return vtable->try_resize(userdata, alloc_base_ptr, cur_size, new_size, src);
  }
  inline bool owns(void* alloc_base_ptr) {
    return vtable->owns(userdata, alloc_base_ptr);
  }
};

struct ArenaTemp;
struct Arena {
  u8* base;
  u8* mem;
  u8* committed;
  usize capacity;

  void* allocate(usize size, usize alignement, const char* src);
  bool try_resize(void* ptr, usize cur_size, usize new_size, const char* src = "<unknown>");
  void deallocate(void* ptr, usize size, const char* src = "<unknown>") {
    try_resize(ptr, size, 0, src);
  }
  bool owns(void* ptr);

  ArenaTemp make_temp();
  void reset() {
    pop_pos(0);
  }

  operator Allocator() {
    return {this, &vtable};
  }

  static const AllocatorVTable vtable;

  // INTERNAL
  u64 pos();
  void pop_pos(u64 pos);
  void deallocate(usize size);
};

inline const AllocatorVTable Arena::vtable{
    .allocate   = [](void* userdata, usize size, usize alignement, const char* src
                ) { return static_cast<Arena*>(userdata)->allocate(size, alignement, src); },
    .deallocate = [](void* userdata, void* alloc_base_ptr, usize size, const char* src
                  ) { return static_cast<Arena*>(userdata)->deallocate(alloc_base_ptr, size, src); },
    .try_resize = [](void* userdata, void* ptr, usize cur_size, usize new_size, const char* src
                  ) { return static_cast<Arena*>(userdata)->try_resize(ptr, cur_size, new_size, src); },
    .owns       = [](void* userdata, void* ptr) { return static_cast<Arena*>(userdata)->owns(ptr); },
};

Arena& arena_alloc(usize capacity = DEFAULT_ARENA_CAPACITY);
void arena_dealloc(Arena& arena);

struct ArenaTemp {
  Arena* arena_;
  u64 old_pos;

  inline void check() {
    DEBUG_ASSERT(arena_ != nullptr);
  }

  void retire() {
    if (arena_ != nullptr) {
      arena_->pop_pos(old_pos);
      arena_ = nullptr;
    }
  }

  operator Allocator() {
    check();
    return *arena_;
  }
  Arena* operator->() {
    check();
    return arena_;
  }
  Arena& operator*() {
    check();
    return *arena_;
  }
  ~ArenaTemp() {
    retire();
  }
};

struct Scratch;
Scratch scratch_get();

struct Scratch {
  Arena* arena_;
  inline void check() {
    DEBUG_ASSERT(arena_ != nullptr);
  }
  void retire();
  operator Allocator() {
    check();
    return *arena_;
  }
  Arena* operator->() {
    check();
    return arena_;
  }
  Arena& operator*() {
    check();
    return *arena_;
  }
  ~Scratch() {
    retire();
  }
};

inline constexpr AllocatorVTable MallocVtable{
    .allocate =
        [](void*, usize size, usize alignement, const char* src) {
          void* mem = calloc(size, 1);
          ASSERT(mem);
          return mem;
        },
    .deallocate = [](void* userdata, void* alloc_base_ptr, usize size, const char* src
                  ) { return free(alloc_base_ptr); },
    .try_resize = [](void* userdata, void* ptr, usize cur_size, usize new_size, const char* src) { return false; },
    .owns       = [](void* userdata, void* ptr) { return false; },
};

enum class AllocatorName {
  General,
  Frame,
};
enum class ArenaName {
  Frame,
};

Allocator get_named_allocator(AllocatorName name);
Arena& get_named_arena(ArenaName name);

} // namespace core
#endif // INCLUDE_CORE_MEMORY_H_
