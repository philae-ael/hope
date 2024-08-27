// IWYU pragma: private

#ifndef INCLUDE_CORE_MATH_H_
#define INCLUDE_CORE_MATH_H_

#include "fwd.h"
#include "platform.h"
#include <cmath>

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
    .flags     = enum_helpers::enum_or(VectorFormatFlags::Multiline, VectorFormatFlags::Alt),
};

str8 to_str8(arena& arena, Vec4 v, VectorFormat format = {});
str8 to_str8(arena& arena, Vec2 v, VectorFormat format = {});

/// Compute
/// - mean,
/// - variance
/// - sample_count
/// Using Welford's algorithm
struct data_series {
  u32 count;
  f32 m;
  f32 M2;

  void add_sample(f32 sample) {
    count      += 1;
    f32 delta   = sample - m;
    m          += delta / (f32)count;
    f32 delta2  = sample - m;
    M2         += delta * delta2;
  }

  /// This is an unbiaised estimator of the population variance
  /// eg for $x_i$ samples of a VA of variance $\sigma$,
  /// $E\left[\text{variance}(x_1, \cdots, x_n)\right] = \sigma$
  f32 variance() const {
    return M2 / ((f32)count - 1);
  }

  /// This is the variance of the samples, it doesn't mean much more i think
  f32 sample_variance() const {
    return M2 / (f32)count;
  }

  /// This is both the mean of the samples and an unbiased estimator of the mean of the population
  /// eg for $x_i$ samples of a VA of mean $m$,
  /// $E\left[\text{mean}(x_1, \cdots, x_n)\right] = m$
  f32 mean() const {
    return m;
  }

  data_series merge(const data_series& other) const {
    return {
        count + other.count,
        m + other.m,
        M2 + other.M2,
    };
  }
};

/// Compute
/// - mean
/// of an online process
/// storing K samples
struct windowed_series {
  core::storage<f32> store;
  usize start{}, count;
  f32 sum{};
  f32 sum2{};

  void add_sample(f32 sample) {
    if (count == store.size) {
      // remove sample due to overflow
      f32 old_sample  = store[start];
      sum            -= old_sample;
      sum2           -= old_sample * old_sample;
      start           = (start + 1) % store.size;
      count          -= 1;
    }
    store[(start + count) % store.size]  = sample;
    count                               += 1;

    sum                                 += sample;
    sum2                                += sample * sample;
  }

  f32 mean() const {
    return sum / f32(count);
  }
  f32 variance() const {
    f32 c = f32(count);
    return (sum / c - sum2) / c;
  }
};

inline bool f32_close_enough(f32 a, f32 b, f32 epsilon = 1e-5f) {
  return fabs(b - a) <= epsilon;
}

} // namespace core
#endif // INCLUDE_CORE_MATH_H_
