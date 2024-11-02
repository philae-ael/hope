#ifndef INCLUDE_CONTAINERS_POOL_H_
#define INCLUDE_CONTAINERS_POOL_H_

#include <core/core.h>

namespace core {

// Meh, just a dummy double linked list
template <class T>
struct pool {

  struct node {
    T data;
    node* next;
    node* prev;
  };

  node* head = nullptr;

  // For those, prev is not maintained
  node* freelist_head = nullptr;

  T& allocate(core::Allocator alloc) {
    node* n;
    if (freelist_head != nullptr) {
      n             = freelist_head;
      freelist_head = n->next;
    } else {
      n = alloc.allocate<node>();
    }

    n->next = head;
    n->prev = nullptr;
    if (head != nullptr) {
      head->prev = n;
    }
    head = n;
    return n->data;
  }

  void deallocate(core::Allocator alloc, T& t) {
    node* n = (node*)&t;

    if (n->prev)
      n->prev->next = n->next;
    if (n->next)
      n->next->prev = n->prev;

    if (alloc.try_resize(n, sizeof(node), 0)) {
      return;
    }

    n->next       = freelist_head;
    freelist_head = n;
  }

  struct queue_iter : cpp_iter<T&, queue_iter> {
    node* n;

    queue_iter(node* n)
        : n(n) {}

    Maybe<T&> next() {
      if (n == nullptr) {
        return Maybe<T&>::None();
      }

      T& t = n->data;
      n    = n->next;
      return t;
    }
  };

  auto iter() {
    return queue_iter{head};
  }
};

}; // namespace core

#endif // INCLUDE_CONTAINERS_POOL_H_
