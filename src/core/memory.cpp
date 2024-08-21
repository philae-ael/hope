#include <atomic>
#include <cstdio>
#include <cstdlib>

#include "../os/memory.h"
#include "core.h"
#include "debug.h"

#ifdef DEBUG
  #include <bit>
#endif

#ifdef SCRATCH_DEBUG
  #define SCRATCH_DEBUG_STMT(f) f
#else
  #define SCRATCH_DEBUG_STMT(f)
#endif

namespace core {

#ifdef ARENA_DEBUG
  #define ARENA_DEBUG_STMT(f) f
#else
  #define ARENA_DEBUG_STMT(f)
#endif

usize arena_page_size() {
#if ARENA_PAGE_SIZE == 0
  return os::mem_page_size();
#else
  return ARENA_PAGE_SIZE;
#endif
}

bool Arena::owns(void* ptr) {
  return ptr >= base && ptr < mem;
}
bool Arena::try_resize(void* ptr, usize cur_size, usize new_size) {
  if (!owns(ptr)) {
    return false;
  }

  if ((uptr)ptr + cur_size != (uptr)mem) {
    return false;
  }

  if (new_size >= cur_size) {
    allocate(new_size - cur_size, 1);
  } else {
    deallocate(cur_size - new_size);
  }

  return true;
}

void* Arena::allocate(usize size, usize alignement) {
  ARENA_DEBUG_STMT(
      printf("Arena: trying to allocate %zu, %zu available\n", size, capacity - (usize)(mem - base))
  );
  DEBUG_ASSERT(std::popcount(alignement) == 1);

  if (size == 0) {
    return nullptr;
  }

  u8* aligned = ALIGN_UP(mem, alignement);
  ARENA_DEBUG_STMT(printf(
      "Arena: padding for %zu byte for alignement, alignement is %zu\n", (usize)(aligned - mem),
      alignement
  ));

  ASSERTM(
      base + capacity >= aligned + size,
      "Arena: out of memory! please implement the commit stuff of ask for more "
      "memory"
  );

  mem = aligned + size;

  ARENA_DEBUG_STMT(printf("Arena: allocated region [%p, %p)\n", aligned, mem));

  if (mem > committed) {
    ARENA_DEBUG_STMT(printf("Arena: Region need being committed!\n"));
    usize page_size = arena_page_size();
    usize size      = ALIGN_UP(mem, page_size) - committed;

    os::mem_allocate(committed, size, os::MemAllocationFlags::Commit);
    ARENA_DEBUG_STMT(printf("Arena: commited region [%p, %p)\n", committed, committed + size));
    committed += size;
  } else {
    ARENA_DEBUG_STMT(
        printf("Arena: no need to commit region already committed until %p\n", committed)
    );
  }

  ASAN_UNPOISON_MEMORY_REGION(aligned, size);
  return aligned;
}

void Arena::deallocate(usize size) {
  ASSERT((usize)(mem - base) >= size);
  mem -= size;
  ASAN_POISON_MEMORY_REGION(mem, capacity - (mem - base));

  usize page_size = arena_page_size();
  if (mem + page_size < committed) {
    u8* aligned = ALIGN_UP(mem, page_size);
    usize size  = ALIGN_DOWN(committed - aligned, page_size);

    os::mem_deallocate(aligned, size, os::MemDeallocationFlags::Decommit);
    committed = aligned;
  }
}

Arena* arena_alloc(usize capacity) {
  usize page_size        = arena_page_size();
  const usize alloc_size = ALIGN_UP(sizeof(Arena) + capacity, page_size);

  u8* memory = (u8*)os::mem_allocate(nullptr, alloc_size, os::MemAllocationFlags::Reserve);
  ARENA_DEBUG_STMT(printf("Arena: got range [%p, %p)\n", memory, memory + alloc_size));
  ASSERT(memory != nullptr);

  os::mem_allocate(memory, page_size, os::MemAllocationFlags::Commit);

  capacity        = alloc_size - sizeof(Arena);

  Arena* arena    = (Arena*)memory;
  arena->capacity = capacity;
  arena->mem = arena->base = memory + sizeof(Arena);
  arena->committed         = memory + page_size;

  ASAN_POISON_MEMORY_REGION(arena->mem, arena->capacity);
  return arena;
}

void arena_dealloc(Arena* arena) {
  if (arena == nullptr) {
    return;
  }

  usize page_size        = arena_page_size();
  const usize alloc_size = ALIGN_UP(sizeof(Arena) + arena->capacity, page_size);

  ARENA_DEBUG_STMT(printf("Arena: realeasing range [%p, %p)\n", arena, arena + alloc_size));
  os::mem_deallocate(arena, alloc_size, os::MemDeallocationFlags::Release);
}

void Arena::pop_pos(u64 pos) {
  ASSERT(mem > (u8*)pos);
  deallocate(mem - (u8*)pos);
}
u64 Arena::pos() {
  return (u64)mem;
}
void ArenaTemp::retire() {
  arena->pop_pos(old_pos);
  arena = nullptr;
}
ArenaTemp Arena::make_temp() {
  return ArenaTemp{
      this,
      pos(),
  };
}

/// NOTES:
/// scratch_* are reentrant
/// The memory barrier part of the atomics are not needed (bc it's thread
/// local) Only the atomicity of the operation is needed BUT it's easier
/// (relaxed should be okay but well)
/// ...
/// Hum... thread local is not core local so i guess it should be fine?
/// Still signals and interrupts?
/// Well after thinking, i think the atomicity is not needed
/// Pfff... a recursive mutex would be better i think
///
/// I decided to remove all atomic access, it's easier and it wont be an issue

thread_local Arena* scratch_storage[SCRATCH_ARENA_AMOUNT]{};
thread_local auto clear_arena_ = defer_builder + [] {
  for (usize i = 0; i < SCRATCH_ARENA_AMOUNT; i++) {
    if (scratch_storage[i] != nullptr) {
      arena_dealloc(scratch_storage[i]);
    }
  }
};

#define SWAP(a, b)  \
  do {              \
    auto tmp = b;   \
    b        = a;   \
    a        = tmp; \
  } while (0)

ArenaScratch scratch_get() {
  if (scratch_storage[0] == nullptr) {
    for (usize i = 0; i < SCRATCH_ARENA_AMOUNT; i++) {
      Arena* arena       = arena_alloc();
      scratch_storage[i] = arena;
    }
  }

  for (usize i = 0; i < SCRATCH_ARENA_AMOUNT; i++) {
    usize j      = SCRATCH_ARENA_AMOUNT - 1 - i;
    Arena* arena = nullptr;
    SWAP(scratch_storage[j], arena);

    if (arena != nullptr) {
      SCRATCH_DEBUG_STMT(
          printf("Scratch: thread %zu, got scratch %zu at %p\n", thread_id(), j, arena->base)
      );

      return ArenaScratch{.arena = arena, .old_pos = arena->pos()};
    }
  }
  panic("no scratch arena available");
}

void scratch_retire(ArenaScratch& scratch) {
  scratch->pop_pos(scratch.old_pos);
  Arena* arena  = scratch.arena;
  scratch.arena = nullptr;

  for (usize i = 0; i < SCRATCH_ARENA_AMOUNT; i++) {
    if (scratch_storage[i] != nullptr) {
      continue;
    }

    Arena* out = nullptr;
    SWAP(out, arena);

    SCRATCH_DEBUG_STMT(
        printf("Scratch: thread %zu, retire scratch %zu at %p\n", thread_id(), i, arena->base)
    );
    return;
  }

  arena_dealloc(arena);
}

std::atomic<usize> thread_counter;

thread_local usize thread_id_ = (usize)-1;

usize thread_id() {
  if (thread_id_ == (usize)-1) {
    thread_id_ = thread_counter.fetch_add(1);
  }
  return thread_id_;
}

} // namespace core
