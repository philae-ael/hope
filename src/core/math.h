// IWYU pragma: private

#ifndef INCLUDE_CORE_MATH_H_
#define INCLUDE_CORE_MATH_H_

#include "fwd.h"

#if GCC || CLANG
typedef f32 __m128 __attribute__((vector_size(16), aligned(16)));
typedef f32 __m256 __attribute__((vector_size(32), aligned(32)));
#else
  #include <immintrin.h>
#endif

namespace core {

using f32x4 = __m128;
using f32x8 = __m128;

struct Vec2 {
  f32 x, y;
  Vec2 operator+(Vec2 other) {
    return {
        x + other.x,
        y + other.y,
    };
  }
  Vec2 operator*(Vec2 other) {
    return {
        x * other.x,
        y * other.y,
    };
  }
  Vec2 operator-(Vec2 other) {
    return {
        x - other.x,
        y - other.y,
    };
  }
  friend Vec2 operator*(f32 lambda, Vec2 self) {
    return {
        lambda * self.x,
        lambda * self.y,
    };
  }
};

struct Vec4 {
  f32 x, y, z, w;
  Vec4 operator+(Vec4 other) {
    return {
        x + other.x,
        y + other.y,
        z + other.z,
        w + other.w,
    };
  }
  Vec4 operator*(Vec4 other) {
    return {
        x * other.x,
        y * other.y,
        z * other.z,
        w * other.w,
    };
  }
  Vec4 operator-(Vec4 other) {
    return {
        x - other.x,
        y - other.y,
        z - other.z,
        w - other.w,
    };
  }
  friend Vec4 operator*(f32 lambda, Vec4 self) {
    return {
        lambda * self.x,
        lambda * self.y,
        lambda * self.z,
        lambda * self.w,
    };
  }
};

struct Vec4x4 {
  f32x4 x, y, z, w;
};

struct Vec4x8 {
  f32x8 x, y, z, w;
};

enum class VectorFormatFlags : u8 {
  PadLeft   = 0x1,
  Multiline = 0x2,
  Alt       = 0x4,
};

struct VectorFormat {
  u8 width     = 0;
  u8 precision = 6;
  VectorFormatFlags flags{};
};

constexpr VectorFormat VectorFormatMultiline{
    .flags = VectorFormatFlags::Multiline,
};
constexpr VectorFormat VectorFormatPretty{
    .width     = 6,
    .precision = 2,
    .flags     = VectorFormatFlags::Multiline | VectorFormatFlags::Alt,
};

str8 to_str8(Arena& arena, Vec4 v, VectorFormat format = {});
str8 to_str8(Arena& arena, Vec2 v, VectorFormat format = {});

} // namespace core
#endif // INCLUDE_CORE_MATH_H_
