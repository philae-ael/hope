// IWYU pragma: private
#ifndef INCLUDE_CORE_BASE_H_
#define INCLUDE_CORE_BASE_H_

#include "platform.h" // IWYU pragma: export
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits.h>
#include <new>

#define FWD(t) static_cast<decltype(t)&&>(t)

template <typename T, size_t N>
char (&_ArraySizeHelper(T (&arr)[N]))[N];
#define ARRAY_SIZE(arr) (sizeof(_ArraySizeHelper(arr)))

#if GCC || CLANG
  #define PRINTF_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
  #define EXPORT __attribute__((visibility("default")))
#elif MSVC
  #define PRINTF_ATTRIBUTE(a, b)
  #define EXPORT
#else
  #error platform not supported
#endif

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
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

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using usize = std::size_t;
using uptr  = std::uintptr_t;

using f32 = float;
static_assert(sizeof(f32) == 4);
using f64 = double;
static_assert(sizeof(f64) == 8);

constexpr usize max_align = alignof(std::max_align_t);

namespace core {

struct Allocator;
struct hstr8 {
  u64 hash;
  usize len;
  const u8* data;

  // This is not a real == operator bc collisions but well, it will be a fun bug to find if a
  // collision occur
  bool operator==(const hstr8& other) const {
    return other.hash == hash;
  }
  hstr8 clone(Allocator alloc);
  const char* cstring(Allocator alloc);
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
  constexpr hstr8 hash() const {
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

  str8 clone(Allocator alloc);
  const char* cstring(Allocator alloc);
};

} // namespace core

#endif // INCLUDE_CORE_BASE_H_
