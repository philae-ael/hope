#include "core/core.h"
#include "prelude.h"
#include <csignal>

log_entry timed_formatter(void* u, arena& arena, log_entry entry) {
  entry         = log_fancy_formatter(nullptr, arena, entry);
  entry.builder = string_builder{}
                      .push(arena, os::time_monotonic(), os::TimeFormat::MM_SS_MMM_UUU_NNN)
                      .push(arena, " ")
                      .append(entry.builder);
  return entry;
}

inline void blackbox(auto& x) {
  asm volatile("nop" : "+r"(x));
}

int main(int argc, char* argv[]) {
  setup_crash_handler();
  log_register_global_formatter(timed_formatter, nullptr);

  LOG_INFO("BUBUBUB");
  int arr[] = {0, 1, 2};
  usize x   = 5000000;
  // blackbox(x);

  return arr[x];
}
