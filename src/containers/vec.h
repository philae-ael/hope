#ifndef INCLUDE_CONTAINERS_VEC_H_
#define INCLUDE_CONTAINERS_VEC_H_

#include "../core/debug.h"
#include "../core/fwd.h"
#include "../core/memory.h"
#include "../core/type_info.h"
#include <cstring>

namespace core {
struct vec {
  storage<u8> store;
  layout_info layout;

  usize size{};

  template <class T>
  vec(identity<T>)
      : layout(default_layout_of<T>()) {}

  vec(layout_info layout)
      : layout(layout) {}

  template <class T>
  vec(storage<T> s, layout_info layout = default_layout_of<T>())
      : store(s.into_bytes())
      , layout(layout) {}

  template <class T>
  void push(arena& ar, const T& t) {
    ASSERT(default_layout_of<T>() == layout);
    push_d(ar, const_storage<u8>::from(unsafe, t));
  }
  template <class T>
  void push(noalloc_t, const T& t) {
    ASSERT(default_layout_of<T>() == layout);
    push_d(noalloc, const_storage<u8>::from(unsafe, t));
  }

  template <class T>
  T pop(noalloc_t) {
    T t;
    pop_d(storage<u8>::from(unsafe, t));
    return t;
  }
  template <class T>
  T pop(arena& ar) {
    T t;
    ASSERT(type_info<T>().layout == layout);
    pop_d(ar, storage<u8>::from(unsafe, t));
    return t;
  }
  void pop_d(arena& ar, storage<u8> d) {
    pop_d(noalloc, d);
    if (capacity() > 8 && capacity() > 2 * size) {
      resize(ar, size);
    }
  }

  void pop_d(noalloc_t, storage<u8> d) {
    DEBUG_ASSERT(d.size == layout.size);
    if (size == 0) {
      panic("Trying to pop empty vec");
    }

    memcpy(d.data, store.data + layout.array(size - 1).size, d.size);

    size -= 1;
  }

  void push_d(arena& ar, const_storage<u8> d) {
    DEBUG_ASSERT(d.size == layout.size);

    if (size == capacity()) {
      resize(ar, MAX(4, capacity() * 2));
    }

    push_d(noalloc, d);
  }

  void push_d(noalloc_t, const_storage<u8> d) {
    ASSERTM(size < capacity(), "No more capacity in vec");
    memcpy(store.data + layout.array(size).size, d.data, layout.size);
    size += 1;
  }

  void resize(arena& ar, usize new_capacity) {
    if (ar.try_resize(store.data, store.size, new_capacity * layout.size, "vec::resize")) {
      store.size = new_capacity * layout.size;
      return;
    }

    auto new_store = ar.allocate_array(layout, new_capacity, "vec::resize");
    memcpy(new_store.data, store.data, size * layout.size);
    store = new_store;
  }

  void move(arena& ar) {
    resize(ar, capacity());
  }

  usize capacity() const {
    return store.size / layout.size;
  }
};

} // namespace core

#endif // INCLUDE_CONTAINERS_VEC_H_
