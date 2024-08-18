#include "core/core.h"
#include "prelude.h"

int main(int argc, char *argv[]) {
  auto *arena = arena_alloc();
  defer { arena_dealloc(arena); };

  string_builder{}.pushf(*arena, "test");
  LOG_BUILDER(LogLevel::Info, with_stacktrace().push("sdasdasda"));
  return 0;
}
