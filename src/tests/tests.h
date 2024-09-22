#ifndef INCLUDE_TESTS_TESTS_H_
#define INCLUDE_TESTS_TESTS_H_

#include "core/core.h"

PRINTF_ATTRIBUTE(2, 3) void tassert(bool, const char* reason, ...);
PRINTF_ATTRIBUTE(1, 2) void tfail(const char* reason, ...);

void register_test(const char* name, void (*)());

struct _register_test {
  template <class F>
  _register_test(const char* name, F f) {
    register_test(name, f);
  }
};

#define TEST__(v, name)                                 \
  static void _test_func_##v();                         \
  const auto v = _register_test{#name, _test_func_##v}; \
  static void _test_func_##v()
#define TEST_(v, name) TEST__(v, name)
#define TEST(name) TEST_(CONCAT(_test, __COUNTER__), name)

#endif // INCLUDE_TESTS_TESTS_H_
