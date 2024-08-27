#ifndef INCLUDE_CONTAINERS_VEC_H_
#define INCLUDE_CONTAINERS_VEC_H_

#include "../core/debug.h"
#include "../core/fwd.h"
#include "../core/memory.h"
#include "../core/type_info.h"
#include <cstring>

namespace core {
struct raw_vec {
  layout_info layout;
  storage<u8> store;

  usize size{};

  constexpr void pop(arena& ar, storage<u8> d) {
    pop(noalloc, d);
    if (capacity() > 8 && capacity() > 2 * size) {
      resize(ar, size);
    }
  }

  constexpr void pop(noalloc_t, storage<u8> d) {
    DEBUG_ASSERT(d.capacity == layout.size);
    if (size == 0) {
      panic("Trying to pop empty raw_vec");
    }

    memcpy(d.data, store.data + layout.array(size - 1).size, d.capacity);
    size -= 1;
  }

  constexpr void push(arena& ar, storage<const u8> d) {
    DEBUG_ASSERT(d.capacity == layout.size);

    if (size == capacity()) {
      resize(ar, MAX(4, capacity() * 2));
    }

    push(noalloc, d);
  }

  constexpr void push(noalloc_t, storage<const u8> d) {
    ASSERTM(size < capacity(), "No more capacity in raw_vec");
    memcpy(store.data + layout.array(size).size, d.data, layout.size);
    size += 1;
  }

  void resize(arena& ar, usize new_capacity) {
    if (ar.try_resize(store.data, store.capacity, new_capacity * layout.size, "raw_vec::resize")) {
      store.capacity = new_capacity * layout.size;
      return;
    }

    auto new_store = ar.allocate_array(layout, new_capacity, "raw_vec::resize");
    memcpy(new_store.data, store.data, size * layout.size);
    store = new_store;
  }

  void move(arena& ar) {
    resize(ar, capacity());
  }

  constexpr usize capacity() const {
    return store.capacity / layout.size;
  }
};

template <class T>
struct vec {
  raw_vec inner{default_layout_of<T>()};

  vec(layout_info layout = default_layout_of<T>())
      : inner(layout) {}
  vec(storage<T> s, layout_info layout = default_layout_of<T>())
      : inner(layout, s.into_bytes()) {}

  constexpr T pop(noalloc_t) {
    T t;
    inner.pop(storage<u8>::from(unsafe, t));
    return t;
  }

  constexpr T pop(arena& ar) {
    T t;
    ASSERT(type_info<T>().layout == inner.layout);
    inner.pop(ar, storage<u8>::from(unsafe, t));
    return t;
  }

  constexpr void push(arena& ar, const T& t) {
    ASSERT(default_layout_of<T>() == inner.layout);
    inner.push(ar, storage<const u8>::from(unsafe, t));
  }

  constexpr void push(noalloc_t, const T& t) {
    ASSERT(default_layout_of<T>() == inner.layout);
    inner.push(noalloc, storage<const u8>::from(unsafe, t));
  }

  void resize(arena& ar, usize new_capacity) {
    return inner.resize(ar, new_capacity);
  }

  void move(arena& ar) {
    return inner.move(ar);
  }

  constexpr usize capacity() const {
    return inner.capacity();
  }

  constexpr usize size() const {
    return inner.size;
  }
};

template <class T, usize N>
vec(T (&a)[N]) -> vec<T>;

} // namespace core

#endif // INCLUDE_CONTAINERS_VEC_H_
