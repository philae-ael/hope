#include "containers/vec.h"
#include "core/fwd.h"
#include "core/log.h"
#include "core/memory.h"
#include "prelude.h"

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
  log_set_global_level(LogLevel::Debug);

  vec v{type_info<u32>().layout};

  auto s = scratch_get();
  for (usize i = 0; i < 500; i++) {
    v.push(*s, u32(i));
  }
  for (usize i = 0; i < 500; i++) {
    v.pop<u32>(*s);
  }
}
