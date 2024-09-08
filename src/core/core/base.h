// IWYU pragma: private
#ifndef INCLUDE_CORE_BASE_H_
#define INCLUDE_CORE_BASE_H_

#include "platform.h" // IWYU pragma: export
#include <cstddef>
#include <cstring>
#include <limits.h>

#define FWD(t) static_cast<decltype(t)&&>(t)

template <typename T, size_t N>
char (&_ArraySizeHelper(T (&arr)[N]))[N];
#define ARRAY_SIZE(arr) (sizeof(_ArraySizeHelper(arr)))

#if GCC || CLANG
  #define PRINTF_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
  #define EXPORT __attribute__((visibility("default")))
#elif MSVC
  #define PRINTF_ATTRIBUTE(a, b)
  #define EXPORT __declspec(dllexport)
#else
  #error platform not supported
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
#define EVAL(a) a

#define ALIGN_MASK_DOWN(x, mask) ((x) & ~(mask))
#define ALIGN_DOWN(x, AMOUNT) ((decltype(x))ALIGN_MASK_DOWN((uptr)(x), AMOUNT - 1))

#define ALIGN_MASK_UP(x, mask) (((x) + (mask)) & (~(mask)))
#define ALIGN_UP(x, AMOUNT) ((decltype(x))ALIGN_MASK_UP((uptr)(x), AMOUNT - 1))

#define TAG(name)                     \
  constexpr struct CONCAT(name, _t) { \
  } name {}

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

namespace core {

struct Arena;
struct hstr8 {
  u64 hash;
  usize len;
  const u8* data;

  // This is not a real == operator bc collisions but well, it will be a fun bug to find if a
  // collision occur
  bool operator==(const hstr8& other) const {
    return other.hash == hash;
  }
  hstr8 clone(Arena& arena);
};

constexpr u64 hash(const u8* data, size_t len) {
  // implementation of fnv1
  u64 h = 0xcbf29ce484222325;
  for (usize i = 0; i < len; i++) {
    h *= 0x100000001b3;
    h ^= u64(data[i]);
  }
  return h;
}
constexpr u64 hash(const char* data, size_t len) {
  // implementation of fnv1
  u64 h = 0xcbf29ce484222325;
  for (usize i = 0; i < len; i++) {
    h *= 0x100000001b3;
    h ^= u64(data[i]);
  }
  return h;
}

TAG(cstr);

struct str8 {
  usize len;
  const u8* data;

  template <size_t len>
  static str8 from(const char (&d)[len]) {
    return from(d, len - 1);
  }
  static str8 from(const char* d, size_t len) {
    return str8{len, reinterpret_cast<const u8*>(d)};
  }
  static str8 from(cstr_t, const char* d) {
    return str8{strlen(d), reinterpret_cast<const u8*>(d)};
  }
  hstr8 hash() const {
    return {
        ::core::hash(data, len),
        len,
        data,
    };
  }

  inline str8 subslice(usize offset) const {
    return {len - offset, data + offset};
  }
  inline str8 subslice(usize offset, const usize len) const {
    return {len, data + offset};
  }

  inline u8 operator[](usize idx) const {
    return data[idx];
  }

  str8 clone(Arena& arena);
  const char* cstring(Arena& arena);
};

} // namespace core

#endif // INCLUDE_CORE_BASE_H_
