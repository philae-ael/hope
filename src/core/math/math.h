#ifndef INCLUDE_CORE_MATH_H_
#define INCLUDE_CORE_MATH_H_

#include <cmath>

#include <core/core/fwd.h>

#include <immintrin.h>
#include <xmmintrin.h>

namespace math {

using f32x4 = __m128;
using u32x4 = __m128i;
using f32x8 = __m128;

namespace consts {
constexpr f32 TAU        = 6.283185307179586f;
constexpr f32 PI         = 3.141592653589793f;
constexpr f32 FRAC_PI_2  = 1.570796326794896f;
constexpr f32 FRAC_PI_4  = 0.785398163397443f;
constexpr f32 DEG_TO_RAD = 0.017453292519943f;
constexpr f32 RAD_TO_DEG = 57.29577951308232f;

constexpr f32 SQRT2        = 1.4142135623730951f;
constexpr f32 FRAC_1_SQRT2 = 0.7071067811865475f;
constexpr f32 SQRT3        = 0.5773502691896258f;
constexpr f32 FRAC_1_SQRT3 = 0.7071067811865475f;
constexpr f32 LN2          = 0.693147180559945f;
} // namespace consts

#define DEGREE(x) ((x) * ::math::consts::DEG_TO_RAD)

/// VECTORS
/// ======

union Vec2 {
  f32 _coeffs[2];
  struct {
    f32 x, y;
  };
  inline constexpr Vec2(f32 x, f32 y)
      : x(x)
      , y(y) {}

  inline constexpr Vec2 operator+(Vec2 other) const {
    return {
        x + other.x,
        y + other.y,
    };
  }
  inline constexpr Vec2 operator*(Vec2 other) const {
    return {
        x * other.x,
        y * other.y,
    };
  }
  inline constexpr Vec2 operator-(Vec2 other) const {
    return {
        x - other.x,
        y - other.y,
    };
  }

  friend inline constexpr Vec2 operator*(Vec2 self, f32 lambda) {
    return {
        lambda * self.x,
        lambda * self.y,
    };
  }
  friend inline constexpr Vec2 operator*(f32 lambda, Vec2 self) {
    return {
        lambda * self.x,
        lambda * self.y,
    };
  }
  inline constexpr f32& operator[](usize i) {
    return _coeffs[i];
  }
  inline constexpr const f32& operator[](usize i) const {
    return _coeffs[i];
  }

  static const Vec2 Zero;
  static const Vec2 X;
  static const Vec2 Y;
};
inline const Vec2 Vec2::Zero{0, 0};
inline const Vec2 Vec2::X{1, 0};
inline const Vec2 Vec2::Y{0, 1};

struct uVec2 {
  u32 x, y;

  inline constexpr uVec2 operator+(uVec2 other) const {
    return {
        x + other.x,
        y + other.y,
    };
  }
  inline constexpr uVec2 operator*(uVec2 other) const {
    return {
        x * other.x,
        y * other.y,
    };
  }
  inline constexpr uVec2 operator-(uVec2 other) const {
    return {
        x - other.x,
        y - other.y,
    };
  }
  friend inline constexpr uVec2 operator*(u32 lambda, uVec2 self) {
    return {
        lambda * self.x,
        lambda * self.y,
    };
  }
};

union Vec4 {
  f32 _coeffs[4];
  f32x4 _vcoeffs;
  struct {
    f32 x, y, z, w;
  };
  struct {
    f32 r, g, b, a;
  };
  inline constexpr Vec4(f32 x, f32 y, f32 z, f32 w)
      : x(x)
      , y(y)
      , z(z)
      , w(w) {}
  inline constexpr Vec4(f32x4 v)
      : _vcoeffs(v) {}

  inline constexpr friend Vec4 operator+(Vec4 lhs, Vec4 rhs) {
    if consteval {
      return {
          lhs.x + rhs.x,
          lhs.y + rhs.y,
          lhs.z + rhs.z,
          lhs.w + rhs.w,
      };
    }
    // Implicit vectorization may already happen... or not idk
    return _mm_add_ps(lhs, rhs);
  }
  inline constexpr friend Vec4& operator+=(Vec4& lhs, Vec4 rhs) {
    return lhs = lhs + rhs;
  }
  inline constexpr Vec4 operator-() const {
    if consteval {
      return {-x, -y, -z, -w};
    }
    return _mm_xor_ps(*this, _mm_set1_ps(-0.0));
  }
  inline constexpr friend Vec4 operator*(Vec4 lhs, Vec4 rhs) {
    if consteval {
      return {
          lhs.x * rhs.x,
          lhs.y * rhs.y,
          lhs.z * rhs.z,
          lhs.w * rhs.w,
      };
    }
    return _mm_mul_ps(lhs, rhs);
  }
  inline constexpr friend Vec4 operator*(f32 lambda, Vec4 self) {
    return Vec4::broadast(lambda) * self;
  }
  inline constexpr friend Vec4 operator*(Vec4 self, f32 lambda) {
    return Vec4::broadast(lambda) * self;
  }
  inline constexpr friend Vec4 operator-(Vec4 lhs, Vec4 rhs) {
    if consteval {
      return {
          lhs.x - rhs.x,
          lhs.y - rhs.y,
          lhs.z - rhs.z,
          lhs.w - rhs.w,
      };
    }
    return _mm_sub_ps(lhs, rhs);
  }

  inline constexpr f32 dot(Vec4 other) const {
    return x * other.x + y * other.y + z * other.z + w * other.w;
  }

  inline constexpr f32& operator[](usize index) {
    return _coeffs[index];
  }
  inline constexpr const f32& operator[](usize index) const {
    return _coeffs[index];
  }
  inline constexpr operator f32x4() const {
    return _vcoeffs;
  }
  inline constexpr static Vec4 broadast(f32 lambda) {
    return {lambda, lambda, lambda, lambda};
  }
  template <usize i, usize j, usize k, usize l>
  inline constexpr Vec4 shuffle() const {
    if consteval {
      return {_coeffs[i], _coeffs[j], _coeffs[k], _coeffs[l]};
    }

    return (f32x4)_mm_shuffle_epi32((u32x4)this->_vcoeffs, _MM_SHUFFLE(l, k, j, i));
  }
  inline constexpr Vec4 normalize() const {
    return *this * (1.f / norm());
  }
  inline constexpr Vec4 normalize_or_zero() const {
    if (auto norm_ = norm(); norm_ != 0.0) {
      return *this * (1.0f / norm_);
    }
    return Zero;
  }

  inline constexpr f32 norm2() const {
    return dot(*this);
  }
  inline constexpr f32 norm() const {
    return std::sqrt(norm2());
  }

  static const Vec4 Zero;
  static const Vec4 One;
  static const Vec4 X;
  static const Vec4 Y;
  static const Vec4 Z;
  static const Vec4 W;
};

inline const Vec4 Vec4::Zero{0, 0, 0, 0};
inline const Vec4 Vec4::One{1, 1, 1, 1};
inline const Vec4 Vec4::X{1, 0, 0, 0};
inline const Vec4 Vec4::Y{0, 1, 0, 0};
inline const Vec4 Vec4::Z{0, 0, 1, 0};
inline const Vec4 Vec4::W{0, 0, 0, 1};

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
    .flags     = core::enum_helpers::enum_or(VectorFormatFlags::Multiline, VectorFormatFlags::Alt),
};

core::str8 to_str8(core::Allocator alloc, Vec4 v, VectorFormat format = {});
core::str8 to_str8(core::Allocator alloc, Vec2 v, VectorFormat format = {});

/// MATRICES
/// ======

TAG(row_major);
TAG(col_major);

// Stored in column-major
union Mat4x4 {
  Vec4 _cols[4];
  f32 _coeffs[16];

  inline constexpr Mat4x4() {}
  inline Mat4x4(col_major_t, f32 coeffs[16]) {
    memcpy(_coeffs, coeffs, sizeof(f32) * 16);
  }
  inline Mat4x4(row_major_t, f32 coeffs[16]) {
    *this = Mat4x4(col_major, coeffs).transpose();
  }
  inline constexpr Mat4x4(col_major_t, Vec4 r1, Vec4 r2, Vec4 r3, Vec4 r4)
      : _cols{r1, r2, r3, r4} {}

  inline constexpr Mat4x4(row_major_t, Vec4 r1, Vec4 r2, Vec4 r3, Vec4 r4) {
    *this = Mat4x4(col_major, r1, r2, r3, r4).transpose();
  }

  inline constexpr f32& at(usize x, usize y) {
    return _cols[y][x];
  }
  inline constexpr const f32& at(usize x, usize y) const {
    return _cols[y][x];
  }
  // inline constexpr f32& operator[](usize x, usize y) {
  //   return _cols[y][x];
  // }
  // inline constexpr const f32& operator[](usize x, usize y) const {
  //   return _cols[y][x];
  // }
  inline constexpr Vec4& col(usize x) {
    return _cols[x];
  }
  inline constexpr const Vec4& col(usize x) const {
    return _cols[x];
  }
  inline constexpr const Mat4x4 transpose() const {
    if consteval {
      return {
          col_major,
          {_coeffs[0], _coeffs[4], _coeffs[8], _coeffs[12]},
          {_coeffs[1], _coeffs[5], _coeffs[9], _coeffs[13]},
          {_coeffs[2], _coeffs[6], _coeffs[10], _coeffs[14]},
          {_coeffs[3], _coeffs[7], _coeffs[11], _coeffs[15]},
      };
    }
    f32x4 m00_m10_m01_m11 = _mm_unpacklo_ps(_cols[0], _cols[1]);
    f32x4 m02_m12_m03_m13 = _mm_unpackhi_ps(_cols[0], _cols[1]);
    f32x4 m20_m30_m21_m31 = _mm_unpacklo_ps(_cols[2], _cols[3]);
    f32x4 m22_m32_m23_m33 = _mm_unpackhi_ps(_cols[2], _cols[3]);
    return {
        col_major,
        _mm_movelh_ps(m00_m10_m01_m11, m20_m30_m21_m31),
        _mm_movehl_ps(m20_m30_m21_m31, m00_m10_m01_m11),
        _mm_movelh_ps(m02_m12_m03_m13, m22_m32_m23_m33),
        _mm_movehl_ps(m22_m32_m23_m33, m02_m12_m03_m13),
    };
  }
  inline constexpr const Mat4x4 operator*(const Mat4x4& B) const {
    return {
        col_major,
        col(0) * B.at(0, 0) + col(1) * B.at(1, 0) + col(2) * B.at(2, 0) + col(3) * B.at(3, 0),
        col(0) * B.at(0, 1) + col(1) * B.at(1, 1) + col(2) * B.at(2, 1) + col(3) * B.at(3, 1),
        col(0) * B.at(0, 2) + col(1) * B.at(1, 2) + col(2) * B.at(2, 2) + col(3) * B.at(3, 2),
        col(0) * B.at(0, 3) + col(1) * B.at(1, 3) + col(2) * B.at(2, 3) + col(3) * B.at(3, 3),
    };
  }

  inline constexpr const Vec4 operator*(const Vec4 v) const {
    return col(0) * v[0] + col(1) * v[1] + col(2) * v[2] + col(3) * v[3];
  }

  constexpr inline friend Mat4x4 operator+(math::Mat4x4 lhs, math::Mat4x4 rhs) {
    return {
        col_major, lhs.col(0) + rhs.col(0), lhs.col(1) + rhs.col(1), lhs.col(2) + rhs.col(2), lhs.col(3) + rhs.col(3),
    };
  }
  constexpr inline friend Mat4x4 operator*(f32 l, math::Mat4x4 rhs) {
    return {col_major, l * rhs.col(0), l * rhs.col(1), l * rhs.col(2), l * rhs.col(3)};
  }
  static const Mat4x4 Zero;
  static const Mat4x4 Id;
};

inline constexpr Mat4x4 Mat4x4::Zero{col_major, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

inline constexpr Mat4x4 Mat4x4::Id{col_major, {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

using Mat4 = Mat4x4;

inline constexpr Mat4x4 projection_matrix_from_far_plane_size(
    f32 near,
    f32 far,
    f32 half_near_plane_width,
    f32 half_near_plane_height
) {
  f32 r = half_near_plane_width;
  f32 t = half_near_plane_height;
  f32 n = near;
  f32 f = far;
  return {
      row_major,
      // clang-format off
      {  n/r  ,   0   ,    0       ,      0        },
      {   0   ,  n/t  ,    0       ,      0        },
      {   0   ,   0   , -f / (f-n) , -f*n / (f-n)  },
      {   0   ,   0   ,   -1       ,      0        }
      // clang-format on
  };
}

// Note: aspect ratio = width / height
inline constexpr Mat4x4 projection_matrix_from_hfov(f32 near, f32 far, f32 hfov, f32 aspect_ratio) {
  auto half_near_plane_width = near * std::tan(hfov / 2);
  return projection_matrix_from_far_plane_size(near, far, half_near_plane_width, half_near_plane_width / aspect_ratio);
}

inline constexpr Mat4x4 projection_matrix_from_vfov(f32 near, f32 far, f32 vfov, f32 aspect_ratio) {
  auto half_near_plane_height = near * std::tan(vfov / 2);
  return projection_matrix_from_far_plane_size(
      near, far, half_near_plane_height * aspect_ratio, half_near_plane_height
  );
}

inline constexpr Mat4x4 translation_matrix(Vec4 translation) {
  translation.w = 1.0;
  return {col_major, Vec4::X, Vec4::Y, Vec4::Z, translation};
}

/// QUATERNIONS
/// ======

union Quat {
  Vec4 v;
  struct {
    f32 x, y, z, w;
  };

  inline constexpr friend Quat operator*(Quat a, Quat b) {
    Vec4 res = a.w * b.v + b.w * a.v;
    res      = res + a.v.shuffle<1, 2, 0, 3>() * b.v.shuffle<2, 0, 1, 3>();
    res      = res - b.v.shuffle<1, 2, 0, 3>() * a.v.shuffle<2, 0, 1, 3>();
    res.w    = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;

    return {res};
  }
  inline constexpr friend Quat& operator*=(Quat& a, Quat b) {
    return a = a * b;
  }

  constexpr inline Quat conjugate() const {
    return {.v = {-x, -y, -z, w}};
  }
  constexpr inline Quat inverse() const {
    return conjugate();
  }
  constexpr inline Quat normalize() const {
    return {v.normalize()};
  }

  static constexpr inline Quat from_axis_angle(Vec4 axis, f32 theta) {
    f32 cos_frac_theta_2 = std::cos(theta / 2);
    f32 sin_frac_theta_2 = std::sin(theta / 2);

    return Quat{
        .v =
            {
                sin_frac_theta_2 * axis.x,
                sin_frac_theta_2 * axis.y,
                sin_frac_theta_2 * axis.z,
                cos_frac_theta_2 * 1,
            }
    }.normalize();
  }

  constexpr inline Vec4 rotate(Vec4 v) {
    v.w      = 0;
    auto res = (*this * Quat{v} * this->conjugate()).v;
    res.w    = 0;
    return res;
  }

  constexpr inline f32 angle() const {
    return 2 * std::atan2(v.norm(), w);
  }
  constexpr inline Vec4 axis() const {
    return Vec4{x, y, z, 0}.normalize();
  }
  constexpr inline Mat4 into_mat4() const {
    f32 s = 1 / v.norm2();

    return Mat4{
        col_major,
        // clang-format off
        { 1 - 2*s*(y*y + z*z),     2*s*(x*y + z*w),     2*s*(x*z - y*w), 0},
        {     2*s*(x*y - z*w), 1 - 2*s*(x*x + z*z),     2*s*(y*z + x*w), 0},
        {     2*s*(x*z + y*w),     2*s*(y*z - x*w), 1 - 2*s*(x*x + y*y), 0},
        // clang-format on
        math::Vec4::W,
    };
  }
  static const Quat Id;
};

inline const Quat Quat::Id{Vec4::W};

/// DATA SERIES
/// ======

// Estimate a percentile using an online alhorithme
// Seems to work
// The algorithm is named FAME (Fast Algorithm for Median Estimation) in a random slide
// (adapted for arbitrary percentile)
// So idk if it really works
// But in my tests it seems to do quite well on median estimation and percentile estimation
// (on a standard normal so idk if it works in a more general setting / with a bimodal
// distribution)
struct percentile_online_algorithm {
  f32 percentile;
  f32 estimated = 0.0;
  f32 step      = 0.0;
  bool init     = false;

  constexpr void add_sample(f32 sample) {
    if (init) {
      if (estimated < sample) {
        estimated += percentile * step;
      } else if (estimated > sample) {
        estimated -= (1.0f - percentile) * step;
      }

      if (std::abs(sample - estimated) < step) {
        step /= 2.0f;
      }
    } else {
      estimated = sample;
      step      = MAX(std::abs(sample), 0.1f);
      init      = true;
    }
  }
};

/// Compute
/// - mean,
/// - variance
/// - sample_count
/// Using Welford's algorithm
struct data_series {
  u32 count;
  f32 m;
  f32 M2;

  constexpr void add_sample(f32 sample) {
    count      += 1;
    f32 delta   = sample - m;
    m          += delta / (f32)count;
    f32 delta2  = sample - m;
    M2         += delta * delta2;
  }

  /// This is an unbiaised estimator of the population variance
  /// eg for $x_i$ samples of a VA of variance $\sigma$,
  /// $E\left[\text{variance}(x_1, \cdots, x_n)\right] = \sigma$
  constexpr f32 variance() const {
    return M2 / ((f32)count - 1);
  }

  /// This is the variance of the samples, it doesn't mean much more i think
  constexpr f32 sample_variance() const {
    return M2 / (f32)count;
  }

  /// This is both the mean of the samples and an unbiased estimator of the mean of the population
  /// eg for $x_i$ samples of a VA of mean $m$,
  /// $E\left[\text{mean}(x_1, \cdots, x_n)\right] = m$
  constexpr f32 mean() const {
    return m;
  }

  constexpr data_series merge(const data_series& other) const {
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

  constexpr void add_sample(f32 sample) {
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

    sum  += sample;
    sum2 += sample * sample;
  }

  constexpr f32 sample_back(usize idx = 0) const {
    return store[(start + count - 1 - idx) % store.size];
  }
  constexpr f32 mean() const {
    return sum / f32(count);
  }
  constexpr f32 variance() const {
    f32 c = f32(count);
    return (sum2 - sum * sum / c) / c;
  }
  constexpr f32 sigma() const {
    return sqrtf(variance());
  }

  constexpr f32 low_99() const {
    percentile_online_algorithm low99{0.99f};
    for (usize i = 0; i < count; i++) {
      low99.add_sample(store[(start + i) % store.size]);
    }
    return low99.estimated;
  }
  constexpr f32 low_95() const {
    percentile_online_algorithm low95{0.95f};
    for (usize i = 0; i < count; i++) {
      low95.add_sample(store[(start + i) % store.size]);
    }
    return low95.estimated;
  }
};

constexpr bool f32_close_enough(f32 a, f32 b, f32 epsilon = 1e-5f) {
  return fabs(b - a) <= epsilon;
}
} // namespace math
#endif // INCLUDE_CORE_MATH_H_
