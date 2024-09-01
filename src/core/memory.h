// IWYU pragma: privatememh
#ifndef INCLUDE_CORE_MEMORY_H_
#define INCLUDE_CORE_MEMORY_H_

#include "debug.h"
#include "fwd.h"
#include "type_info.h"

#ifndef SCRATCH_ARENA_AMOUNT
  #define SCRATCH_ARENA_AMOUNT 3
#endif

#ifndef DEFAULT_ARENA_CAPACITY
  #define DEFAULT_ARENA_CAPACITY GB(1) // it's virtual memory, we have plenty
#endif

#ifndef ARENA_BLOCK_ALIGNEMENT
  #define ARENA_BLOCK_ALIGNEMENT max_align_v
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
struct LayoutInfo;
struct ArenaTemp;
struct Arena {
  u8* base;
  u8* mem;
  u8* committed;
  usize capacity;

  void* allocate(
      usize size,
      usize alignement = ARENA_BLOCK_ALIGNEMENT,
      const char* src  = "<unknown>"
  );
  void* allocate(LayoutInfo layout, const char* src = "<unknown>") {
    return allocate(layout.size, layout.alignement, src);
  }
  bool try_resize(void* ptr, usize cur_size, usize new_size, const char* src = "<unknown>");

  template <class T>
  T* allocate() {
    return static_cast<T*>(allocate(default_layout_of<T>(), type_name<T>()));
  }

  storage<u8> allocate_array(
      LayoutInfo layout,
      usize element_count,
      const char* src = "<unknown>"
  ) {
    auto arr_layout = layout.array(element_count);
    return {
        arr_layout.size,
        (u8*)allocate(arr_layout, src),
    };
  }
  template <class T>
  storage<T> allocate_array(usize element_count) {
    auto arr_layout = default_layout_of<T>().array(element_count);
    return {
        element_count,
        (T*)allocate(arr_layout, type_name<T>()),
    };
  }

  u64 pos();
  void pop_pos(u64 pos);
  bool owns(void* ptr);

  ArenaTemp make_temp();

private:
  void deallocate(usize size);
};

Arena& arena_alloc(usize capacity = DEFAULT_ARENA_CAPACITY);
void arena_dealloc(Arena& arena);

struct ArenaTemp {
  Arena* arena_;
  u64 old_pos;

  void retire();

  Arena* operator->() {
    check();
    return arena_;
  }
  Arena& operator*() {
    check();
    return *arena_;
  }
  void check() {
    ASSERT(arena_ != nullptr);
  }
};

struct scratch;
scratch scratch_get();
void scratch_retire(scratch&);

struct scratch {
  Arena* arena_;
  u64 old_pos;

  void retire() {
    check();
    scratch_retire(*this);
  }
  Arena* operator->() {
    check();
    return arena_;
  }
  Arena& operator*() {
    check();
    return *arena_;
  }

  void check() {
    ASSERTM(arena_ != nullptr, "scratch has already been already retired");
  }
};

template <class T, usize size_ = sizeof(T), usize alignement_ = alignof(T)>
struct pool {
  static constexpr usize size       = size_;
  static constexpr usize alignement = alignement_;

  Arena* arena_;

  struct node {
    node* next;
  };
  static_assert(sizeof(T) >= sizeof(node));
  static_assert(alignof(T) >= alignof(node));

  // free list implemented as a queue
  node tail{};
  node* head = &tail;

  T* allocate() {
    if (tail.next != nullptr) {
      node* n   = tail.next;
      tail.next = n->next;
      return (T*)n;
    }
    return arena_->allocate<T>();
  }
  void deallocate(T* t) {
    if (arena_->try_resize(t, size, 0, "pool")) {
      return;
    }

    node* n    = (node*)t;
    n->next    = nullptr;
    head->next = n;
    head       = n;
  }
};

} // namespace core
#endif // INCLUDE_CORE_MEMORY_H_
