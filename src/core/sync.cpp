#include "core.h"

#include <atomic>

namespace core::sync {

static std::atomic<usize> thread_counter{};
static thread_local usize thread_id_ = (usize)-1;
usize thread_id() {
  if (thread_id_ == (usize)-1) {
    thread_id_ = thread_counter.fetch_add(1);
  }
  return thread_id_;
}

} // namespace core::sync
