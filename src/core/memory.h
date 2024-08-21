// IWYU pragma: private
#ifndef INCLUDE_CORE_MEMORY_H_
#define INCLUDE_CORE_MEMORY_H_

#include "base.h"
#include "debug.h"

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
  #define ARENA_PAGE_SIZE MB(1)
#endif // !ARENA_PAGE_SIZE

#define ALIGN_MASK_DOWN(x, mask) ((x) & ~(mask))
#define ALIGN_DOWN(x, AMOUNT) ((decltype(x))ALIGN_MASK_DOWN((uptr)(x), AMOUNT - 1))

#define ALIGN_MASK_UP(x, mask) (((x) + (mask)) & (~(mask)))
#define ALIGN_UP(x, AMOUNT) ((decltype(x))ALIGN_MASK_UP((uptr)(x), AMOUNT - 1))

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
  #include <sanitizer/asan_interface.h>
  #define ASAN_POISON_MEMORY_REGION(addr, size) __asan_poison_memory_region((addr), (size))
  #define ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
  #define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
  #define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

namespace core {
struct ArenaTemp;
struct arena {
  u8* base;
  u8* mem;
  u8* committed;
  usize capacity;

  void* allocate(usize size, usize alignement = ARENA_BLOCK_ALIGNEMENT);
  bool try_resize(void* ptr, usize cur_size, usize new_size);

  template <class T>
  T* allocate() {
    return static_cast<T*>(allocate(sizeof(T), alignof(T)));
  }

  u64 pos();
  void pop_pos(u64 pos);
  bool owns(void* ptr);

  ArenaTemp make_temp();

private:
  void deallocate(usize size);
};

arena* arena_alloc(usize capacity = DEFAULT_ARENA_CAPACITY);
void arena_dealloc(arena* arena);

struct ArenaTemp {
  arena* arena_;
  u64 old_pos;

  void retire();

  arena* operator->() {
    check();
    return arena_;
  }
  arena& operator*() {
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
  arena* arena_;
  u64 old_pos;

  void retire() {
    check();
    scratch_retire(*this);
  }
  arena* operator->() {
    check();
    return arena_;
  }
  arena& operator*() {
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

  arena* arena_;

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
    if (arena_->try_resize(t, size, 0)) {
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
