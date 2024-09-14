#ifndef INCLUDE_CONTAINERS_VEC_H_
#define INCLUDE_CONTAINERS_VEC_H_

#include "../core/debug.h"
#include "../core/fwd.h"
#include "../core/memory.h"
#include "../core/type_info.h"
#include <cstring>

namespace core {
template <class T>
struct vec {
  storage<T> store{};
  usize size_{};

  vec() {}
  template <usize len>
  vec(T (&a)[len])
      : store(storage{a})
      , size_(len) {}

  vec(storage<T> s)
      : store(s)
      , size_(s.size) {}

  constexpr T pop(noalloc_t) {
    if (size_ == 0) {
      panic("Trying to pop empty vec");
    }

    T t    = store[size_ - 1];
    size_ -= 1;
    return t;
  }

  constexpr T pop(Arena& ar) {
    T t = pop(noalloc);
    if (capacity() > 8 && capacity() > 2 * size()) {
      set_capacity(ar, size_);
    }
    return t;
  }

  constexpr void push(noalloc_t, const T& t) {
    ASSERT(store.data != nullptr);
    ASSERT(size() <= capacity());
    store[size_]  = t;
    size_        += 1;
  }

  constexpr void push(Arena& ar, const T& t) {
    if (size() >= capacity()) {
      set_capacity(ar, MAX(4, capacity() * 2));
    }

    push(noalloc, t);
  }

  void set_size(usize new_size) {
    ASSERT(new_size <= capacity());
    size_ = new_size;
  }
  void set_capacity(Arena& ar, usize new_capacity, bool try_grow = true) {
    if (try_grow &&
        ar.try_resize(store.data, store.size, new_capacity * sizeof(T), "vec::resize")) {
      store.size = new_capacity;
      return;
    }

    auto new_store = ar.allocate_array<T>(new_capacity, "vec::resize");
    memcpy(new_store.data, store.data, size() * sizeof(T));
    store = new_store;
  }

  vec clone(Arena& ar) {
    vec copy = *this;
    copy.set_capacity(ar, capacity(), false);
    return copy;
  }

  constexpr usize capacity() const {
    return store.size;
  }

  constexpr usize size() const {
    return size_;
  }
  constexpr T* data() {
    return store.data;
  }
  constexpr const T* data() const {
    return store.data;
  }

  constexpr operator storage<T>() {
    return {size(), data()};
  }
  constexpr operator storage<const T>() const {
    return {size(), data()};
  }

  constexpr const T& operator[](usize i) const {
    ASSERT(i < size());
    return *(data() + i);
  }
  constexpr T& operator[](usize i) {
    ASSERT(i < size());
    return *(data() + i);
  }

  auto indices() const {
    return range{0zu, size()};
  }
  auto iter() {
    return storage<T>{*this}.iter();
  }
  auto iter() const {
    return storage<const T>{*this}.iter();
  }
};

} // namespace core

#endif // INCLUDE_CONTAINERS_VEC_H_
