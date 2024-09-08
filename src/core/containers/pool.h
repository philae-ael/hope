#ifndef INCLUDE_CONTAINERS_POOL_H_
#define INCLUDE_CONTAINERS_POOL_H_

#include <core/core.h>

namespace core {
template <class T, usize size_ = sizeof(T), usize alignement_ = alignof(T)>
struct pool {
  static constexpr usize size       = size_;
  static constexpr usize alignement = alignement_;

  struct node {
    node* next;
  };
  static_assert(sizeof(T) >= sizeof(node));
  static_assert(alignof(T) >= alignof(node));

  // free list implemented as a queue
  node tail{};
  node* head = &tail;

  T& allocate(core::Arena& arena) {
    if (tail.next != nullptr) {
      node* n   = tail.next;
      tail.next = n->next;
      return (T*)n;
    }
    return *arena.allocate<T>();
  }
  void deallocate(core::Arena& arena, T& t) {
    if (arena.try_resize(t, size, 0, "pool")) {
      return;
    }

    node* n    = (node*)&t;
    n->next    = nullptr;
    head->next = n;
    head       = n;
  }
};

}; // namespace core

#endif // INCLUDE_CONTAINERS_POOL_H_
