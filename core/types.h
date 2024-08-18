// IWYU pragma: private, include "core/core.h"

#ifndef INCLUDE_CORE_TYPE_H_
#define INCLUDE_CORE_TYPE_H_

#include <limits.h>
#include <stddef.h>
#include <type_traits>

#include "platform.h"
#define FWD(t) static_cast<decltype(t) &&>(t)

#if GCC || CLANG
#define PRINTF_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
#else
#define PRINTF_ATTRIBUTE(a, b)
#endif

#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)
#define GB(x) ((x) * 1024 * 1024 * 1024)

#define STRINGIFY_(c) #c
#define STRINGIFY(c) STRINGIFY_(c)
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

using u8 = unsigned char;
static_assert(CHAR_BIT == 8);
using u16 = unsigned short;
static_assert(sizeof(u16) == 2);
using u32 = unsigned int;
static_assert(sizeof(u32) == 4);
using u64 = unsigned long;
static_assert(sizeof(u64) == 8);

using s8 = signed char;
static_assert(sizeof(s8) == 1);
using s16 = signed short;
static_assert(sizeof(s16) == 2);
using s32 = signed int;
static_assert(sizeof(s32) == 4);
using s64 = signed long;
static_assert(sizeof(s64) == 8);
using usize = size_t;
using uptr = u64;
static_assert(sizeof(uptr) == sizeof((void *)0));

using f32 = float;
static_assert(sizeof(f32) == 4);

using f64 = double;
static_assert(sizeof(f64) == 8);

namespace core {

namespace tag {
struct inplace_t {};
constexpr inplace_t inplace{};
} // namespace tag

template <class T> struct Maybe {
  enum class Discriminant : u8 { None, Some } d;
  union {
    T t;
  };

  Maybe(T t) : d(Discriminant::Some), t(t) {}
  template <class... Args>
  Maybe(tag::inplace_t, Args &&...args)
      : d(Discriminant::Some), t(FWD(args)...) {}
  Maybe() : d(Discriminant::None) {}

  inline bool is_some() { return d == Discriminant::Some; }
  inline bool is_none() { return d == Discriminant::None; }
  inline T &value() { return t; }
  inline T &operator*() { return value(); }
  inline T *operator->() { return &t; }
};

template <class T>
  requires std::is_enum_v<T>
constexpr T operator|(T lhs, T rhs) {
  return static_cast<T>(static_cast<std::underlying_type<T>::type>(lhs) |
                        static_cast<std::underlying_type<T>::type>(rhs));
}

template <class T>
  requires std::is_enum_v<T>
constexpr T operator&(T lhs, T rhs) {
  return static_cast<T>(static_cast<std::underlying_type<T>::type>(lhs) &
                        static_cast<std::underlying_type<T>::type>(rhs));
}

template <class F> struct defer_t : F {
  ~defer_t() { F::operator()(); }
};

struct defer_builder_t {
  auto operator+(auto f) { return defer_t{FWD(f)}; }
};

#define defer auto CONCAT(defer__, __COUNTER__) = core::defer_builder_t{} + [&]

} // namespace core

#endif // INCLUDE_CORE_TYPE_H_
