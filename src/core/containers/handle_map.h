#ifndef INCLUDE_CONTAINERS_INDEXMAP_H_
#define INCLUDE_CONTAINERS_INDEXMAP_H_

#include <core/containers/vec.h>
#include <core/core.h>

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
    handle h;

    if (free_list_head.freenode.next_free_index != 0x00FFFFFF) {
      auto idx   = free_list_head.freenode.next_free_index;
      auto& next = reinterpret_cast<freelist_node&>(handles[idx]);

      free_list_head.freenode.next_free_index = next.freenode.next_free_index;
      next.handle.index                       = max_handle_idx++;
      h                                       = handle_inner{next.handle.generation, idx};
    } else {
      handles.push(alloc, handle_inner{0, max_handle_idx++});
      h = handle_inner{0, u32(handles.size() - 1)};
    }

    return h;
  }

  // This is O(n) if last handle is not given!
  Command free_handle(handle h, core::Maybe<handle> last_handle = {}) {
    auto handle = handle_inner(h);
    ASSERTM(handle.index < handles.size(), "corrupt handle detected");
    ASSERTM(handle.generation == handles[handle.index].generation, "trying to free an invalid handle");

    u32 idx = handles[handle.index].index;

    handles[handle.index].generation        = handle.generation + 1;
    handles[handle.index].index             = free_list_head.freenode.next_free_index;
    free_list_head.freenode.next_free_index = handle.index;

    max_handle_idx -= 1;
    if (idx == max_handle_idx) {
      return {.delete_last = {}};
    }

    if (last_handle.is_none()) {
      for (auto& h : handles.iter()) {
        if (h.index == max_handle_idx) {
          h.index = idx;
          return {.swap_delete_last{.idx = idx}};
        }
      }
      core::panic("This should be unreachable");
    } else {
      auto& h = handles[handle_inner(*last_handle).index];
      ASSERTM(h.index == max_handle_idx, "invalid last_handle!");
      h.index = idx;
      return {.swap_delete_last{.idx = idx}};
    }
  }

  core::Maybe<u32> resolve(handle h) {
    handle_inner handle = h;
    ASSERTM(handle.index < handles.size(), "corrupt handle detected");
    if (handle.generation != handles[handle.index].generation) {
      return {};
    }
    return u32(handles[handle.index].index);
  }

  void reset(core::Allocator alloc) {
    handles.reset(alloc);
    max_handle_idx = 0;
  }

private:
  struct handle_inner {
    u32 generation : 8;
    u32 index : 24;
    constexpr handle_inner(u32 generation, u32 index)
        : generation(generation)
        , index(index) {}
    handle_inner(handle h)
        : generation(((u32)h & 0xFF000000) >> 24)
        , index((u32)h & 0x00FFFFFF) {}
    operator handle() {
      return static_cast<handle>(generation << 24 | index);
    }
  };
  static_assert(sizeof(handle_inner) == sizeof(handle), "index_map requires u32 handles");

  core::vec<handle_inner> handles;
  u32 max_handle_idx = 0;

  union freelist_node {
    handle_inner handle;
    struct {
      u32 generation : 8;
      u32 next_free_index : 24;
    } freenode;
  };
  freelist_node free_list_head = {.freenode = {0, 0xFFFFFF}};

  template <class T, class handle>
  friend struct handle_map;
};

// A struct that maps handles to elements of T
// Implemented using index_map
// insertion: O(1)
// deletion: O(1)
// access: O(1)
// Can be iterated with good cache locality => This is the principal way this
// should be used Not ordered Caveat:
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
    ASSERTM(data.size() == hma.max_handle_idx, "");
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

  void reset(core::Allocator alloc) {
    handles.reset(alloc);
    data.reset(alloc);
    hma.reset(alloc);
  }
};

} // namespace core

#endif // INCLUDE_CONTAINERS_INDEXMAP_H_
