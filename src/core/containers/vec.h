#ifndef INCLUDE_CONTAINERS_VEC_H_
#define INCLUDE_CONTAINERS_VEC_H_

#include "../core/fwd.h"
#include "../core/memory.h"
#include "core/core.h"
#include <cstring>
#include <initializer_list>

namespace core {
template <class T>
struct vec {
  storage<T> store{};
  usize size_{};

  vec() {}
  template <class S, usize len>
  vec(S (&a)[len])
      : store(a)
      , size_(len) {}

  template <class S, usize len>
  vec(clear_t, S (&a)[len])
      : store(a)
      , size_(0) {}

  template <class S>
  vec(storage<S> s)
      : store(s)
      , size_(s.size) {}
  template <class S>
  vec(clear_t, storage<S> s)
      : store(s)
      , size_(0) {}

  vec(std::initializer_list<T> s)
      : store(s)
      , size_(s.size()) {}

  constexpr T pop(noalloc_t) {
    if (size_ == 0) {
      panic("Trying to pop empty vec");
    }

    size_--;
    return store[size_];
  }

  constexpr T pop(Allocator alloc) {
    T t = pop(noalloc);
    if (capacity() > 8 && capacity() > 2 * size()) {
      set_capacity(alloc, size_);
    }
    return t;
  }

  constexpr void push(noalloc_t, T t) {
    ASSERT(store.data != nullptr);
    ASSERT(size() < capacity());
    store[size_]  = t;
    size_        += 1;
  }

  constexpr void push(Allocator alloc, T t) {
    if (size() >= capacity()) {
      set_capacity(alloc, MAX(4, capacity() * 2));
    }

    push(noalloc, t);
  }

  void set_size(usize new_size) {
    ASSERT(new_size <= capacity());
    size_ = new_size;
  }
  void set_capacity(Allocator alloc, usize new_capacity, bool try_grow = true) {
    if (try_grow && alloc.try_resize((void*)store.data, store.size, new_capacity * sizeof(T), "vec::resize")) {
      store.size = new_capacity;
      return;
    }

    if (new_capacity == 0) {
      store = {};
      return;
    }

    auto new_store = alloc.allocate_array<T>(new_capacity, "vec::resize");
    memcpy((void*)new_store.data, store.data, size() * sizeof(T));
    alloc.deallocate((void*)store.data, store.size);
    store = new_store;
  }

  void reset(noalloc_t) {
    set_size(0);
  }
  void reset(Allocator alloc) {
    reset(noalloc);
    set_capacity(alloc, 0);
  }

  vec clone(Allocator alloc) {
    vec copy = *this;
    copy.set_capacity(alloc, capacity(), false);
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

  // iterator is not invalidated when the current item is destroyed
  auto iter_rev() {
    return storage<T>{*this}.iter_rev();
  }
  auto iter_rev() const {
    return storage<const T>{*this}.iter_rev();
  }

  Maybe<T&> last() {
    return size() > 0 ? (*this)[size() - 1] : core::None<T&>();
  }
  Maybe<const T&> last() const {
    return size() > 0 ? (*this)[size() - 1] : core::None<const T&>();
  }

  // Does not invalidate a reverse iterator!
  // Useful to delete elements from a vector while iterating it backward
  T swap_last_pop(usize idx) {
    SWAP((*this)[idx], (*this)[size() - 1]);
    return pop(core::noalloc);
  }
};

template <class T>
vec(storage<T>) -> vec<T>;

} // namespace core

#endif // INCLUDE_CONTAINERS_VEC_H_
