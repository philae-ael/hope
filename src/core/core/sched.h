#ifndef INCLUDE_CORE_SCHED_H_
#define INCLUDE_CORE_SCHED_H_

#include <concepts>
#include <core/containers/pool.h>
#include <core/core/memory.h>

namespace core {
struct TaskQueue;

enum class TaskReturn { Yield, Stop };
enum class TaskStatus { Active, Stopped };

template <class Data = void>
using task_func = TaskReturn (*)(Data*, TaskQueue*);

struct Task {
  TaskStatus status;
  void* data;
  task_func<> func;

  TaskReturn run(TaskQueue* queue) {
    return (*func)(data, queue);
  }

  template <class Data = void, std::convertible_to<task_func<Data>> F>
  static Task from(F f, Data* data = nullptr) {
    return Task{
        TaskStatus::Active,
        (void*)data,
        task_func<>(static_cast<task_func<Data>>(f)),
    };
  }
};

struct TaskQueue {
  core::pool<Task> tasks;

  Task* allocate_job() {
    return &tasks.allocate(core::get_named_allocator(core::AllocatorName::General));
  }
  void deallocate_job(Task* task) {
    return tasks.deallocate(core::get_named_allocator(core::AllocatorName::General), *task);
  }

  void run() {
    for (auto& task : tasks.iter()) {
      if (task.status != TaskStatus::Active)
        continue;

      switch (task.run(this)) {
      case TaskReturn::Stop:
        task.status = TaskStatus::Stopped;
        break;
      case TaskReturn::Yield:
        break;
      }
    }
  }
};

TaskQueue* default_task_queue();
} // namespace core
#endif // INCLUDE_CORE_SCHED_H_
