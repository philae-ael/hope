#include "tests.h"

#include <core/core/future.h>

TEST(Sched) {
  Scheduler sched{};
  sched.run_main_thread();
}
