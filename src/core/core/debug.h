// IWYU pragma: private

#ifndef INCLUDE_CORE_ASSERT_H_
#define INCLUDE_CORE_ASSERT_H_

#include "base.h"

#if GCC || CLANG
  #define TRAP asm volatile("int $3")
#elif MSVC
  #define TRAP __debugbreak()
#else
  #error NOT IMPLEMENTED
#endif

#define ASSERT(cond)                                           \
  do {                                                         \
    if (!(cond)) [[unlikely]] {                                \
      ::core::panic("assertion `%s` failed", STRINGIFY(cond)); \
    }                                                          \
  } while (0)

#define ASSERTEQ(a, b)                                                                                          \
  do {                                                                                                          \
    auto __a          = (a);                                                                                    \
    decltype(__a) __b = (b);                                                                                    \
    if (!(__a == __b)) [[unlikely]] {                                                                           \
      ::core::panic("assertion `%s == %s` failed: `%g != %g`", STRINGIFY(a), STRINGIFY(b), (f32)__a, (f32)__b); \
    }                                                                                                           \
  } while (0)

#define ASSERTM(cond, fmt, ...)                                                                 \
  do {                                                                                          \
    if (!(cond)) [[unlikely]] {                                                                 \
      ::core::panic("assertion `%s` failed:\n" fmt, STRINGIFY(cond) __VA_OPT__(, __VA_ARGS__)); \
    }                                                                                           \
  } while (0)

#ifdef DEBUG
  #define DEBUG_STMT(f) f
#else
  #define DEBUG_STMT(f)
#endif

#define DEBUG_ASSERT(...) DEBUG_STMT(ASSERT(__VA_ARGS__))
#define DEBUG_ASSERTM(...) DEBUG_STMT(ASSERTM(__VA_ARGS__))

#ifdef DEBUG
  #define ASSERT_INVARIANT(x) ASSERT(x)
#else
  #define ASSERT_INVARIANT(x) [[assume(x)]]
#endif

#define DROP_4_ARG(a1, a2, a3, a4, ...) (__VA_ARGS__)
#define todo(...)                                                                                                  \
  core::panic __VA_OPT__(DROP_4_ARG)(                                                                              \
      "function %s has not been implemented in file %s:%d", __func__, __FILE__, __LINE__ __VA_OPT__(, __VA_ARGS__) \
  )

namespace core {

struct str8;

struct source_location {
  str8 file;
  str8 func;
  u32 line;
};

#define CURRENT_SOURCE_LOCATION                                          \
  ::core::source_location {                                              \
    ::core::str8::from(__FILE__), ::core::str8::from(__func__), __LINE__ \
  }

[[noreturn]] void panic(const char* msg, ...) PRINTF_ATTRIBUTE(1, 2);

void dump_backtrace(usize skip = 1);

void setup_crash_handler();

inline void blackbox(auto& x) {
#if LINUX
  void* ptr = &x;
  asm volatile("nop" : "+rm"(ptr));
#else
    // blackbox will do nothing
#endif
}

} // namespace core

#endif // INCLUDE_CORE_ASSERT_H_
