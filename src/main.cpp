#include "prelude.h"
#include <thread>

log_entry timed_formatter(void* u, arena& arena, log_entry entry) {
  entry         = log_fancy_formatter(nullptr, arena, entry);
  entry.builder = string_builder{}
                      .push(arena, os::time_monotonic(), os::TimeFormat::MM_SS_MMM_UUU_NNN)
                      .push(arena, " ")
                      .append(entry.builder);
  return entry;
}

int main(int argc, char* argv[]) {
  log_register_global_formatter(timed_formatter, nullptr);
  std::jthread{[] {
    LOG_INFO("H");
  }}.join();

  return 0;
}
