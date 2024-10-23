#ifndef INCLUDE_CORE_FUTURE_H_
#define INCLUDE_CORE_FUTURE_H_

#include "core/core.h"
#include "core/core/fwd.h"
#include <atomic>
#include <bit>
#include <core/containers/vec.h>

struct Task {
  void* data;
  void (*func)(void* data);

  void run();
};

class Scheduler;
using thread_id = core::handle_t<class Thread>;

// implementation of the deque from Hendler, D. and Shavit, N. ‘Non-Blocking Steal-Half Work Queues’.
// NOTES:
// 2.2.1
//
// TODO:
// Make it growable? Maybe not?!
struct steal_half_work_queue {
  static constexpr usize deq_size = 512;
  static_assert(deq_size < u32(-1));

  // has to be a power of 2
  static constexpr usize steal_amount = 2 << 5;

  core::array<Task, deq_size> deq;
  struct steal_range_t {
    // Generational idx to prevent ABA
    u32 tag;

    // First item of the deque
    u32 top;

    // Last item of the steal_range
    // u32(-1) 0xFFFFFFFF denote an empty steal range
    std::atomic<u32> steal_last = u32(-1);

    bool operator==(const steal_range_t& other) const = default;
    steal_range_t& operator=(const steal_range_t& other) {
      tag        = other.tag;
      top        = other.top;
      steal_last = other.steal_last.load();
      return *this;
    }

    steal_range_t(const steal_range_t& other) {
      *this = other;
    }
  } prev_steal_range, steal_range;

  // the index of the first empty item at the bottom of the deque
  // if eq to top, the deque is empty
  u32 bot;

  inline bool empty() const {
    return bot == steal_range.top;
  }

  inline bool full() const {
    return (bot + 1) % deq_size == steal_range.top;
  }
  inline usize len() const {
    return (steal_range.top - bot) % deq_size;
  }

  enum class push_res { Ok, Full };

  // Should be called by local thread only
  push_res push_bottom(Task task) {
    if (full())
      return push_res::Full;

    deq[bot] = task;

    bot = (bot + 1) % deq_size;

    bool ok = false;
    if (std::has_single_bit(len())) {
      u32 old_last = steal_range.steal_last;
      u32 new_last = steal_range.top + MAX((u32)len() >> 1, 1) - 1;
      ok           = steal_range.steal_last.compare_exchange_weak(old_last, new_last);
    } else if (prev_steal_range != steal_range) {
      u32 old_last = steal_range.steal_last;
      u32 new_last = steal_range.top + MAX(std::bit_ceil((u32)len()) >> 2, 1) - 1;
      ok           = steal_range.steal_last.compare_exchange_weak(old_last, new_last);
    }

    if (ok) {
      prev_steal_range = steal_range;
    }

    return push_res::Ok;
  }

  union pop_res {
    enum class Tag { Ok, Empty, Fail } tag;
    struct {
      Tag tag = Tag::Ok;
      Task task;
    } ok;
    struct {
      Tag tag = Tag::Empty;
    } empty;
    struct {
      Tag tag = Tag::Fail;
    } fail;
  };
  pop_res pop_bottom() {
    if (empty()) {
      return {.empty{}};
    }
    bool ok = false;
    if (std::has_single_bit(len())) {
      u32 old_last = steal_range.steal_last;
      u32 new_last = steal_range.top + MAX((u32)len() >> 1, 1) - 1;
      ok           = steal_range.steal_last.compare_exchange_weak(old_last, new_last);
    } else if (prev_steal_range != steal_range) {
      u32 old_last = steal_range.steal_last;
      u32 new_last = steal_range.top + MAX(std::bit_ceil((u32)len()) >> 2, 1) - 1;
      ok           = steal_range.steal_last.compare_exchange_weak(old_last, new_last);
    }

    if (ok) {
      prev_steal_range = steal_range;
    } else {
      return {.fail{}};
    }

    bot                           = (bot - 1) % deq_size;
    Task task                     = deq[bot];
    steal_range_t old_steal_range = steal_range;
    bool old_steal_range_contains_bot =
        (old_steal_range.steal_last - old_steal_range.top) % deq_size <= (bot - old_steal_range.top) % deq_size;
    if (!old_steal_range_contains_bot) {
      return {.ok{.task = task}};
    } else if (old_steal_range.steal_last == u32(-1)) {
      bot = 0;
      return {.empty{}};
    } else {
      bool stolen  = false;
      u32 old_last = old_steal_range.steal_last;
      u32 new_last = u32(-1);
      stolen       = steal_range.steal_last.compare_exchange_weak(old_last, new_last);
      if (stolen) {
        // task not stolen
        return {.ok{.task = task}};
      } else {
        // task stolen
        return {.empty{}};
      }
    }
  }

  u32 try_to_steal(steal_half_work_queue& queue, u32 plen) {
    return 0;
  }
};

class Thread {
public:
  thread_id id;
  Scheduler* sched;
  steal_half_work_queue deq;

  inline bool should_balance() {
    // See 2.2.1 for different strategies
    return deq.empty();
  }

private:
};

class Scheduler {
  core::vec<Thread> threads;
};

#endif // INCLUDE_CORE_FUTURE_H_
