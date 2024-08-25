#ifndef INCLUDE_CONTAINERS_VEC_H_
#define INCLUDE_CONTAINERS_VEC_H_

#include "../core/log.h"
#include "../core/memory.h"
#include "../core/type_info.h"
#include <cstring>

namespace core {
struct vec {
  layout_info layout;

  usize size;
  usize capacity;
  void* base;

  template <class T>
  void push(arena& ar, const T& t) {
    LOG_BUILDER(LogLevel::Trace, push(default_layout_of<T>()));
    ASSERT(default_layout_of<T>() == layout);
    push_d(ar, const_data::from(t));
  }

  template <class T>
  T pop() {
    T t;
    pop_d(data::from(t));
    return t;
  }
  template <class T>
  T pop(arena& ar) {
    T t;
    ASSERT(type_info<T>().layout == layout);
    pop_d(ar, data::from(t));
    return t;
  }
  void pop_d(arena& ar, data d) {
    pop_d(d);
    if (capacity > 8 && capacity > 2 * size) {
      resize(ar, size);
    }
  }

  void pop_d(data d) {
    DEBUG_ASSERT(d.size == layout.size);
    if (size == 0) {
      panic("Trying to pop empty vec");
    }

    memcpy(d.data, (u8*)base + layout.array(size - 1).size, d.size);

    size -= 1;
  }

  void push_d(arena& ar, const_data d) {
    DEBUG_ASSERT(d.size == layout.size);

    if (size >= capacity) {
      resize(ar, MAX(4, capacity * 2));
    }

    ASSERT(size < capacity);

    memcpy((u8*)base + layout.array(size).size, d.data, layout.size);
    size += 1;
  }

  void resize(arena& ar, usize new_capacity) {
    LOG_DEBUG("resizing vec from capacity %zu to capacity %zu", capacity, new_capacity);
    if (ar.try_resize(base, capacity * layout.size, new_capacity * layout.size, "vec::resize")) {
      capacity = new_capacity;
      return;
    }

    void* mem = ar.allocate(layout.array(new_capacity), "vec::resize");
    memcpy(mem, base, size * layout.size);
    base     = mem;
    capacity = new_capacity;
  }

  void move(arena& ar) {
    resize(ar, capacity);
  }
};

} // namespace core

#endif // INCLUDE_CONTAINERS_VEC_H_
