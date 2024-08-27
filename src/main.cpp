#include "core/core.h"
#include "core/debug.h"
#include "os/os.h"

using namespace core;
using namespace core::literals;

log_entry timed_formatter(void* u, arena& arena, core::log_entry entry) {
  entry         = log_fancy_formatter(nullptr, arena, entry);
  entry.builder = string_builder{}
                      .push(arena, os::time_monotonic(), os::TimeFormat::MM_SS_MMM_UUU_NNN)
                      .push(arena, " ")
                      .append(entry.builder);
  return entry;
}

int main(int argc, char* argv[]) {
  setup_crash_handler();
  log_register_global_formatter(timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Debug);

  f32 s[2];
  windowed_series w{s};
  w.add_sample(5.0);
  ASSERTEQ(w.mean(), 5.0f);
  ASSERTEQ(w.count, 1);
  w.add_sample(3.0);
  ASSERTEQ(w.count, 2);
  ASSERTEQ(w.mean(), 4.0);
  w.add_sample(3.0);
  ASSERTEQ(w.variance(), 0.0f);
  ASSERTEQ(w.mean(), 3.0f);
  ASSERTEQ(w.count, 2);
}
