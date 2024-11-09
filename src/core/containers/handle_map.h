#ifndef INCLUDE_CONTAINERS_INDEXMAP_H_
#define INCLUDE_CONTAINERS_INDEXMAP_H_

#include <core/containers/vec.h>
#include <core/core.h>
#include <core/core/types.h>

#include <bit>

namespace core {

// This should be thought as a container adapter
//
// Map handles into contiguous indices
// Allow for lookup while keeping data iterable and defragmented
template <class handle_type = core::handle_t<void, u32>>
class handle_map_adapter {
public:
  using handle = handle_type;
  union Command {
    enum class Tag : u8 { Nop, SwapDeleteLast, DeleteLast } tag = Tag::Nop;
    struct {
      Tag tag = Tag::SwapDeleteLast;
      u32 idx;
    } swap_delete_last;
    struct {
      Tag tag = Tag::DeleteLast;
    } delete_last;
  };

  handle new_handle(core::Allocator alloc) {
    ASSERTM((max_handle_idx & 0x00FFFFFF) != 0x00FFFFFF, "bip bip too many indices (max supported is 16'777'215)");

    // WARN: O(n) BAD! use a free list!
    for (auto [idx, h] : core::enumerate{handles.iter()}) {
      if (h->idx() == (u32(-1) & 0x00FFFFFF)) {
        h->set_idx(max_handle_idx++);
        return handle_inner{h->generation(), static_cast<u32>(idx)}.into();
      }
    }
    handles.push(alloc, handle_inner{0, max_handle_idx++});
    return handle_inner{0, u32(handles.size() - 1)}.into();
  }

  // This is O(n) if last handle is not given!
  Command free_handle(handle h, core::Maybe<handle> last_handle = {}) {
    auto handle = handle_inner::from(h);
    ASSERTM(handle.idx() < handles.size(), "corrupt handle detected");
    if (handle.generation() != handles[handle.idx()].generation()) {
      return {};
    }

    u32 idx = handles[handle.idx()].idx();

    handles[handle.idx()].set_generation(handle.generation() + 1);
    handles[handle.idx()].set_idx(u32(-1));

    max_handle_idx -= 1;
    if (idx == max_handle_idx) {
      return {.delete_last = {}};
    }

    if (last_handle.is_none()) {
      for (auto& h : handles.iter()) {
        if (h.idx() == max_handle_idx) {
          h.set_idx(idx);
          return {.swap_delete_last{.idx = idx}};
        }
      }
      core::panic("This should be unreachable");
    } else {
      auto& h = handles[handle_inner::from(*last_handle).idx()];
      ASSERTM(h.idx() == max_handle_idx, "invalid last_handle!");
      h.set_idx(idx);
      return {.swap_delete_last{.idx = idx}};
    }
  }

  core::Maybe<u32> resolve(handle h) {
    auto handle = handle_inner::from(h);
    ASSERTM(handle.idx() < handles.size(), "corrupt handle detected");
    if (handle.generation() != handles[handle.idx()].generation()) {
      return {};
    }
    return handles[handle.idx()].idx();
  }

  void deallocate(core::Allocator alloc) {
    handles.deallocate(alloc);
    max_handle_idx = 0;
    freelist_tail  = free_list_dangling;
  }

private:
  struct handle_inner {
    bool operator==(const handle_inner& other) const   = default;
    handle_inner& operator=(const handle_inner& other) = default;
    handle_inner(const handle_inner& other)            = default;
    u32 inner;

    inline constexpr handle_inner(u8 gen, u32 idx)
        : inner(((u32)gen << 24) | (idx & 0x00FFFFFF)) {}

    handle into() const {
      return std::bit_cast<handle>(*this);
    }
    constexpr static handle_inner from(handle h) {
      return std::bit_cast<handle_inner>(h);
    }
    inline constexpr u32 idx() const {
      return inner & 0x00FFFFFF;
    }
    inline constexpr u8 generation() const {
      return (inner & 0xFF000000) >> 24;
    }
    inline constexpr void set_generation(u8 gen) {
      inner = idx() | ((u32)gen << 24);
    }
    inline constexpr void set_idx(u32 idx) {
      inner = ((u32)generation() << 24) | (idx & 0x00FFFFFF);
    }
  };
  static_assert(sizeof(handle_inner) == sizeof(handle), "index_map requires u32 handles");

  struct freelist_node : handle_inner {};
  static_assert(sizeof(handle) >= sizeof(freelist_node));

  static constexpr freelist_node free_list_dangling{{(u8)-1, (u32)-1}};
  freelist_node freelist_tail{free_list_dangling};

  core::vec<handle_inner> handles;
  u32 max_handle_idx = 0;
};
template class handle_map_adapter<>;

// A struct that maps handles to elements of T
// Implemented using index_map
// insertion: O(1)
// deletion: O(1)
// access: O(1)
// Can be iterated with good cache locality => This is the principal way this should be used
// Not ordered
// Caveat:
// - update is 2 array lookups (2 cache miss) -> random access is quite bad
// - stores 1 * T + 2*u32 for each elememts
template <class T, class Handle = core::handle_t<T, u32>>
struct handle_map {
  using handle             = Handle;
  using handle_map_adapter = core::handle_map_adapter<handle>;

  // There are 2 vec! Be careful with arenas
  // It allows for O(1) delete in exchange for O(N) additional storage added
  core::vec<handle> handles{};
  core::vec<T> data{};
  handle_map_adapter hma{};

  handle insert(core::Allocator alloc, T&& t) {
    auto handle = hma.new_handle(alloc);
    auto idx    = hma.resolve(handle).expect("handle can't be resolved afer being created");
    ASSERTM(idx == data.size(), "The index map does strange unexpected things");

    handles.push(alloc, handle);
    data.push(alloc, FWD(t));
    return handle;
  }

  core::Maybe<const T&> get(handle h) const {
    auto idx = hma.resolve(h);
    if (idx.is_none()) {
      return {};
    }
    return data[idx.value()];
  }

  core::Maybe<T&> get(handle h) {
    auto idx = hma.resolve(h);
    if (idx.is_none()) {
      return {};
    }
    return data[idx.value()];
  }
  core::Maybe<T&> operator[](handle h) {
    return get(h);
  }

  void destroy(handle h) {
    auto command = hma.free_handle(h, handles.last().copied());
    switch (command.tag) {
    case handle_map_adapter::Command::Tag::SwapDeleteLast:
      data.swap_last_pop(command.swap_delete_last.idx);
      handles.swap_last_pop(command.swap_delete_last.idx);
      break;
    case handle_map_adapter::Command::Tag::DeleteLast:
      data.pop(noalloc);
      handles.pop(noalloc);
      break;
    case handle_map_adapter::Command::Tag::Nop:
      break;
    }
  }

  auto iter() const {
    return data.iter();
  }
  auto iter() {
    return data.iter();
  }

  auto iter_enumerate() {
    return core::zipiter{handles.iter(), data.iter()};
  }
  auto iter_rev_enumerate() {
    return core::zipiter{handles.iter_rev(), data.iter_rev()};
  }

  void deallocate(core::Allocator alloc) {
    handles.deallocate(alloc);
    data.deallocate(alloc);
    hma.deallocate(alloc);
  }
};

} // namespace core

#endif // INCLUDE_CONTAINERS_INDEXMAP_H_
