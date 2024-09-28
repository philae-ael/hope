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
class index_map {
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
    if (freelist_tail == free_list_dangling) {
      ASSERTM((max_handle_idx & 0x00FFFFFF) != 0x00FFFFFF, "bip bip too many indices (max supported is 16'777'215)");
      handles.push(alloc, handle_inner{0, max_handle_idx++});
      return handle_inner{0, u32(handles.size() - 1)}.into();
    }

    u32 idx       = freelist_tail.idx();
    u8 generation = handles[idx].generation();
    if (idx != (u32(-1) & 0xFFFFFF)) {
      freelist_tail = freelist_node{handles[idx]};
    } else {
      freelist_tail = free_list_dangling;
    }
    handles[idx].set_idx(max_handle_idx++);

    return handle_inner{generation, idx}.into();
  }

  // This is O(n) if last handle is not given!
  Command free_handle(handle h, core::Maybe<handle> last_handle = {}) {
    auto handle = handle_inner::from(h);
    ASSERTM(handle.idx() < handles.size(), "corrupt handle detected");
    if (handle.generation() != handles[handle.idx()].generation()) {
      return {};
    }

    handles[handle.idx()].set_generation(handles[handle.idx()].generation() + 1);

    u32 idx = handles[handle.idx()].idx();

    if (freelist_tail == free_list_dangling) {
      handles[handle.idx()].set_idx(u32(-1));
      freelist_tail = freelist_head = freelist_node{handle};
    } else {
      freelist_head.set_idx(handle.idx());
    }

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
  static_assert(sizeof(handle_inner) == sizeof(handle));

  struct freelist_node : handle_inner {};
  static_assert(sizeof(handle) >= sizeof(freelist_node));

  static constexpr freelist_node free_list_dangling{{(u8)-1, (u32)-1}};
  freelist_node freelist_head{free_list_dangling};
  freelist_node freelist_tail{free_list_dangling};

  core::vec<handle_inner> handles;
  u32 max_handle_idx = 0;
};

template <class T>
struct linear_growable_pool_with_generational_handles {
  using handle    = core::handle_t<T, u32>;
  using index_map = core::index_map<handle>;

  core::vec<T> data;

  // There are 2 vec! Be careful with arenas
  // It allows for O(1) delete in exchange for O(N) additional storage added
  core::vec<handle> handles;
  index_map im;

  handle insert(core::Allocator alloc, T&& t) {
    auto handle = im.new_handle(alloc);
    auto idx    = im.resolve(handle).expect("handle can't be resolved afer being created");
    ASSERTM(idx == data.size(), "The index map does strange unexpected things");

    handles.push(alloc, handle);
    data.push(alloc, FWD(t));
    return handle;
  }

  core::Maybe<const T&> get(handle h) const {
    auto idx = im.resolve(h);
    if (idx.is_none()) {
      return {};
    }
    return data[idx.value()];
  }

  core::Maybe<T&> get(handle h) {
    auto idx = im.resolve(h);
    if (idx.is_none()) {
      return {};
    }
    return data[idx.value()];
  }

  void destroy(handle h) {
    auto command = im.free_handle(h, handles.last().copied());
    switch (command.tag) {
    case index_map::Command::Tag::SwapDeleteLast:
      SWAP(data[command.swap_delete_last.idx], data[data.size() - 1]);
      [[fallthrough]];
    case index_map::Command::Tag::DeleteLast:
      data.pop(noalloc);
      handles.pop(noalloc);
      break;
    case index_map::Command::Tag::Nop:
      break;
    }
  }

  auto iter() const {
    return data.iter();
  }
  auto iter() {
    return data.iter();
  }
};

} // namespace core

#endif // INCLUDE_CONTAINERS_INDEXMAP_H_
