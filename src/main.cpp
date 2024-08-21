#include "prelude.h"

log_entry timed_formatter(void* u, Arena& arena, log_entry entry) {
  entry         = log_fancy_formatter(nullptr, arena, entry);
  entry.builder = string_builder{}
                      .push(arena, os::time_monotonic(), os::TimeFormat::MM_SS_MMM_UUU_NNN)
                      .push(arena, " ")
                      .append(entry.builder);
  return entry;
}

int main(int argc, char* argv[]) {
  log_register_global_formatter(timed_formatter, nullptr);
  LOG_INFO("H");

  auto arena = arena_alloc();
  arena_dealloc(arena);

  return 0;
}
