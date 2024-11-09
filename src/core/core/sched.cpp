#include "sched.h"

namespace core {
static TaskQueue default_task_queue_;

EXPORT TaskQueue* default_task_queue() {
  return &default_task_queue_;
}
} // namespace core
