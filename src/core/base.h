// IWYU pragma: private
#ifndef INCLUDE_CORE_BASE_H_
#define INCLUDE_CORE_BASE_H_

#include "platform.h" // IWYU pragma: export
#include <cstddef>
#include <limits.h>

#define FWD(t) static_cast<decltype(t)&&>(t)

#if GCC || CLANG
  #define PRINTF_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
#else
  #define PRINTF_ATTRIBUTE(a, b)
#endif

#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#define SWAP(a, b)  \
  do {              \
    auto tmp = b;   \
    b        = a;   \
    a        = tmp; \
  } while (0)

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)
#define GB(x) ((x) * 1024 * 1024 * 1024)

#define STRINGIFY_(c) #c
#define STRINGIFY(c) STRINGIFY_(c)
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define ALIGN_MASK_DOWN(x, mask) ((x) & ~(mask))
#define ALIGN_DOWN(x, AMOUNT) ((decltype(x))ALIGN_MASK_DOWN((uptr)(x), AMOUNT - 1))

#define ALIGN_MASK_UP(x, mask) (((x) + (mask)) & (~(mask)))
#define ALIGN_UP(x, AMOUNT) ((decltype(x))ALIGN_MASK_UP((uptr)(x), AMOUNT - 1))

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
using uptr  = u64;
static_assert(sizeof(uptr) == sizeof((void*)0));

using f32 = float;
static_assert(sizeof(f32) == 4);

using f64 = double;
static_assert(sizeof(f64) == 8);

constexpr usize max_align_v = alignof(std::max_align_t);

#endif // INCLUDE_CORE_BASE_H_
