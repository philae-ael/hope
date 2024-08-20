// IWYU pragma: private

#ifndef INCLUDE_CORE_MEMORY_H_
#define INCLUDE_CORE_MEMORY_H_

#include "debug.h"
#include "types.h"

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
#define ALIGN_DOWN(x, AMOUNT) ALIGN_MASK_DOWN((uptr)(x), AMOUNT - 1)

#define ALIGN_MASK_UP(x, mask) (((x) + (mask)) & (~(mask)))
#define ALIGN_UP(x, AMOUNT) ALIGN_MASK_UP((uptr)(x), AMOUNT - 1)

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
struct Arena {
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

Arena* arena_alloc(usize capacity = DEFAULT_ARENA_CAPACITY);
void arena_dealloc(Arena* arena);

struct ArenaTemp {
  Arena* arena;
  u64 old_pos;

  void retire();

  Arena* operator->() {
    check();
    return arena;
  }
  Arena& operator*() {
    check();
    return *arena;
  }
  void check() {
    ASSERT(arena != nullptr);
  }
};

struct ArenaScratch;
ArenaScratch scratch_get();
void scratch_retire(ArenaScratch&);

struct ArenaScratch {
  Arena* arena;
  u64 old_pos;

  void retire() {
    check();
    scratch_retire(*this);
  }
  Arena* operator->() {
    check();
    return arena;
  }
  Arena& operator*() {
    check();
    return *arena;
  }

  void check() {
    ASSERTM(arena != nullptr, "scratch has already been already retired");
  }
};

namespace detail {
template <class T, class underlying>
struct handle_impl {
  enum class handle_t : underlying {};
};
} // namespace detail

template <class T, class underlying = usize>
using handle_t = detail::handle_impl<T, underlying>::handle_t;

template <class T, class underlying>
underlying to_underlying(handle_t<T, underlying> h) {
  return static_cast<underlying>(h);
}

template <class T, usize size_ = sizeof(T), usize alignement_ = alignof(T)>
struct Pool {
  static constexpr usize size       = size_;
  static constexpr usize alignement = alignement_;

  Arena* arena;

  struct node {
    node* next;
  };
  static_assert(sizeof(T) >= sizeof(node));
  static_assert(alignof(T) >= alignof(node));

  // free list implemented as a queue
  node tail{};
  node* head = &tail;

  T* alloc() {
    if (tail.next != nullptr) {
      node* n   = tail.next;
      tail.next = n->next;
      return (T*)n;
    }
    return arena->allocate<T>();
  }
  void dealloc(T* t) {
    if (arena->try_resize(t, size, 0)) {
      return;
    }

    node* n    = (node*)t;
    n->next    = nullptr;
    head->next = n;
    head       = n;
  }
};

template struct Pool<u64>;

} // namespace core
#endif // INCLUDE_CORE_MEMORY_H_
