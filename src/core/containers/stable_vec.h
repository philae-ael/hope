#ifndef INCLUDE_CONTAINERS_STABLE_VEC_H_
#define INCLUDE_CONTAINERS_STABLE_VEC_H_
#include "core/core.h"
#include "core/core/base.h"

namespace core {
// Like a vec but coded as a linked list of storage of predetermined size
// It is useful whenever an arena is used for allocation
// the main advantages of a stable vec:
// - pointer are preserved after an insert / delete
// - data is not copied when there is not enough space <- does not fragment an arena
template <class T, usize BlockSize = KB(1)>
class stable_vec {
public:
  void push(core::Allocator alloc, T t) {
    if (head == nullptr) {
      head = tail = alloc.allocate<Block>();
    }

    if (tail->header.size == Block::elem_per_block) {
      auto* old_tail        = tail;
      old_tail->header.next = tail = alloc.allocate<Block>();
      tail->header.prev            = old_tail;
    }

    auto* block = tail;
    while (block->header.size == 0 && block->header.prev != nullptr &&
           block->header.prev->header.size != Block::elem_per_block) {
      block = block->header.prev;
    }

    block->items[block->header.size]  = t;
    block->header.size               += 1;
  }

  T pop() {
    ASSERTM(tail != nullptr, "trying to pop an empty vec");

    auto* block = tail;
    while (block->header.size == 0 && block->header.prev != nullptr) {
      block = block->header.prev;
    }

    ASSERTM(block->header.size != 0, "trying to pop an empty vec");

    block->header.size -= 1;
    return block->items[block->header.size];
  }
  usize capacity() const {
    usize cap   = 0;
    auto* block = head;
    while (block != nullptr) {
      cap   += Block::elem_per_block;
      block  = block->header.next;
    }
    return cap;
  }

  T& at(usize idx) {
    Block* block = head;
    while (idx >= Block::elem_per_block && block != nullptr) {
      idx   -= Block::elem_per_block;
      block  = block->header.next;
    }

    ASSERT(block != nullptr);

    return block->items[idx];
  }

  const T& at(usize idx) const {
    Block* block = head;
    while (idx >= Block::elem_per_block && block != nullptr) {
      idx   -= Block::elem_per_block;
      block  = block->header.next;
    }

    ASSERT(block != nullptr);

    return block->items[idx];
  }

  auto& operator[](usize idx) {
    return at(idx);
  }
  auto& operator[](usize idx) const {
    return at(idx);
  }

  auto iter() {
    return iterator{head, 0};
  }

  void deallocate(Allocator alloc) {
    Block* block = head;
    while (block != nullptr) {
      auto* next = block->header.next;
      alloc.deallocate(block);
      block = next;
    }

    head = tail = nullptr;
  }

private:
  struct Block {
    // Maybe align on cache line first elem of items? and put header at the end
    struct BlockHeader {
      Block* next = nullptr;
      Block* prev = nullptr;
      usize size  = 0;
    } header;
    static constexpr usize elem_per_block = (BlockSize - sizeof(BlockHeader)) / sizeof(T);
    core::array<T, elem_per_block> items{};
  };
  static_assert(Block::elem_per_block >= 1);
  static_assert(sizeof(Block) <= BlockSize && sizeof(Block) + sizeof(T) > BlockSize);

  Block* head = nullptr;
  Block* tail = nullptr;

  struct iterator : cpp_iter<T&, iterator> {
    using Item = T&;
    Block* b;
    usize block_idx;

    iterator(Block* b, usize block_idx)
        : b(b)
        , block_idx(block_idx) {}

    core::Maybe<T&> next() {

      if (b != nullptr && block_idx == Block::elem_per_block) {
        b         = b->header.next;
        block_idx = 0;
      }
      if (b == nullptr || block_idx >= b->header.size) {
        return {};
      }

      return b->items[block_idx++];
    }
  };
};
} // namespace core
#endif // INCLUDE_CONTAINERS_STABLE_VEC_H_
