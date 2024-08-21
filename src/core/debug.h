// IWYU pragma: private

#ifndef INCLUDE_CORE_ASSERT_H_
#define INCLUDE_CORE_ASSERT_H_

#include "base.h"
#include <source_location>

#if GCC || CLANG
  #define TRAP asm volatile("int $3")
#else
  #error NOT IMPLEMENTED
#endif

#define ASSERT(cond)                                           \
  do {                                                         \
    if (!(cond)) [[unlikely]] {                                \
      ::core::panic("assertion `" STRINGIFY(cond) "` failed"); \
    }                                                          \
  } while (0)

#define ASSERTM(cond, fmt, ...)                                       \
  do {                                                                \
    if (!(cond)) [[unlikely]] {                                       \
      ::core::panic(                                                  \
          "assertion `" STRINGIFY(cond) "` failed:\n" fmt,            \
          ::std::source_location::current() __VA_OPT__(, __VA_ARGS__) \
      );                                                              \
    }                                                                 \
  } while (0)

#ifdef DEBUG
  #define DEBUG_STMT(f) f
#else
  #define DEBUG_STMT(f)
#endif

#define DEBUG_ASSERT(...) DEBUG_STMT(ASSERT(__VA_ARGS__))
#define DEBUG_ASSERTM(...) DEBUG_STMT(ASSERTM(__VA_ARGS__))

#define todo(...) ::core::panic(__VA_ARGS__);

#define NORETURN [[noreturn]]

namespace core {

NORETURN
PRINTF_ATTRIBUTE(1, 3)
void panic(const char* msg, std::source_location loc = std::source_location::current(), ...);

void dump_backtrace(int skip = 1);

} // namespace core

#endif // INCLUDE_CORE_ASSERT_H_
