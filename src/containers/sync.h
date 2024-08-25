// IWYU pragma: private

#ifndef INCLUDE_CORE_SYNC_H_
#define INCLUDE_CORE_SYNC_H_

#include "../core/fwd.h"
#include <atomic>

namespace core::sync {
usize thread_id();

// Treiber stack
template <class T, usize alignement = alignof(T)>
struct stack {
  struct stack_node {
    stack_node* next;
    T data;
  };

  using tagged_ptr = TaggedPtr<stack_node, alignement>;
  std::atomic<tagged_ptr> top{};

  void push(stack_node* node) {
    tagged_ptr cur_top = top;
    do {
      node->next = cur_top.ptr();
    } while (!std::atomic_compare_exchange_weak(&top, &cur_top, {node, cur_top.tag()}));
  }

  stack_node* pop() {
    tagged_ptr cur_top = top;
    stack_node* n;
    do {
      if (cur_top.ptr() == nullptr) {
        return nullptr;
      }

      n = cur_top.ptr()->next;
    } while (!std::atomic_compare_exchange_weak(&top, &cur_top, {n, cur_top.tag() + 1}));
    return cur_top.ptr();
  }

  bool empty() {
    return top.load().ptr() == nullptr;
  }
};

} // namespace core::sync

#endif // INCLUDE_CORE_SYNC_H_
