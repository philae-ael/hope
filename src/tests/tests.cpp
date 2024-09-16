#include "tests.h"
#include "core/containers/vec.h"
#include "core/core.h"
#include <cstdio>
#include <cstdlib>

struct test_failure {
  const char* reason;
};

void vfail(const char* fmt, va_list ap) {
  va_list ap_copy;
  va_copy(ap_copy, ap);
  usize len = (usize)vsnprintf(nullptr, 0, fmt, ap_copy);
  va_end(ap_copy);
  const char* buffer = (const char*)malloc(len + 1);
  vsnprintf((char*)buffer, len + 1, fmt, ap);

  throw test_failure{buffer};
}
void tfail(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfail(fmt, ap);
  va_end(ap);
}
void tassert(bool ok, const char* fmt, ...) {
  if (!ok) {
    va_list ap;
    va_start(ap, fmt);
    vfail(fmt, ap);
    va_end(ap);
  }
}

struct test {
  const char* name;
  void (*test)();
};

static core::Arena* arena{};
static core::vec<test>& get_all_tests() {
  static core::vec<test> all_tests;
  return all_tests;
};

void register_test(const char* name, void (*test)()) {
  if (arena == nullptr) {
    arena = &core::arena_alloc();
  }

  get_all_tests().push(*arena, {name, test});
}

int main() {
  core::setup_crash_handler();
  log_register_global_formatter(core::log_timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Trace);

  core::source_location loc{core::str8::from("tests"), core::str8::from("tests"), u32(-1)};
  usize failed = 0;
  for (auto& test : get_all_tests().iter()) {
    core::log_builder(core::LogLevel::Info, loc).pushf("RUNNING tests %s", test.name).emit();
    try {
      test.test();
    } catch (test_failure& f) {
      if (strlen(f.reason) > 0) {
        core::log_builder(core::LogLevel::Error, loc)
            .pushf("RUNNING tests %s: FAILED (%s)", test.name, f.reason)
            .emit();
      } else {
        core::log_builder(core::LogLevel::Error, loc)
            .pushf("RUNNING tests %s: FAILED", test.name)
            .emit();
      }
      failed += 1;
      continue;
    }
    core::log_builder(core::LogLevel::Info, loc).pushf("RUNNING tests %s: OK", test.name).emit();
  }

  if (failed > 0) {
    core::log_builder(core::LogLevel::Info, loc).pushf("%zu tests failed", failed).emit();
  } else {
    core::log_builder(core::LogLevel::Info, loc).pushf("all tests passed!").emit();
  }
  return (int)failed;
}
