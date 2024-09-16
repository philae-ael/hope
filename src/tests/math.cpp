#include "tests.h"
#include <core/core/math.h>
using namespace core;

TEST(matmul) {
  for (auto i : range{0zu, 4zu}.iter()) {
    for (auto j : range{0zu, 4zu}.iter()) {
      for (auto k : range{0zu, 4zu}.iter()) {
        for (auto l : range{0zu, 4zu}.iter()) {
          Mat4 M   = Mat4::Zero;
          Mat4 N   = Mat4::Zero;
          M[i, j]  = 1.0f;
          N[k, l]  = 1.0f;
          Mat4 res = M * N;

          for (auto m : range{0zu, 4zu}.iter()) {
            for (auto n : range{0zu, 4zu}.iter()) {
              f32 expected = (m == i && j == k && l == n) ? 1.0f : 0.0f;
              f32 got      = res[m, n];
              tassert(
                  expected == got, "e_%zu_%zu * e_%zu_%zu at %zu %zu, expected %g, got %g", i, j, k,
                  l, m, n, expected, got
              );
            }
          }
        }
      }
    }
  }
}

bool mat_eq(Mat4 a, Mat4 b) {
  for (auto i : range{0zu, 4zu}.iter()) {
    for (auto j : range{0zu, 4zu}.iter()) {
      if (!f32_close_enough(a[i, j], b[i, j])) {
        return false;
      }
    }
  }
  return true;
}

TEST(matlin) {
  for (auto i : range{0zu, 4zu}.iter()) {
    for (auto j : range{0zu, 4zu}.iter()) {
      for (auto k : range{0zu, 4zu}.iter()) {
        for (auto l : range{0zu, 4zu}.iter()) {
          Mat4 M   = Mat4::Zero;
          Mat4 N   = Mat4::Zero;
          M[i, j]  = 1.0f;
          N[k, l]  = 1.0f;
          Mat4 res = M + 2.f * N;

          for (auto m : range{0zu, 4zu}.iter()) {
            for (auto n : range{0zu, 4zu}.iter()) {
              f32 expected =
                  (m == i && j == n ? 1.0f : 0.0f) + 2 * (m == k && l == n ? 1.0f : 0.0f);
              f32 got = res[m, n];
              tassert(
                  expected == got, "e_%zu_%zu + 2*e_%zu_%zu at %zu %zu, expected %g, got %g", i, j,
                  k, l, m, n, expected, got
              );
            }
          }
        }
      }
    }
  }
}

TEST(quatmulelem) {
  tassert(((Quat(Vec4::X) * Quat(Vec4::X)).v + Vec4::W).norm2() <= 0.001, "X*X = -W");
  tassert(((Quat(Vec4::X) * Quat(Vec4::Y)).v - Vec4::Z).norm2() <= 0.001, "X*Y = +Z");
  tassert(((Quat(Vec4::X) * Quat(Vec4::Z)).v + Vec4::Y).norm2() <= 0.001, "X*Z = -Y");
  tassert(((Quat(Vec4::X) * Quat(Vec4::W)).v - Vec4::X).norm2() <= 0.001, "X*W = +X");

  tassert(((Quat(Vec4::Y) * Quat(Vec4::X)).v + Vec4::Z).norm2() <= 0.001, "Y*X = -Z");
  tassert(((Quat(Vec4::Y) * Quat(Vec4::Y)).v + Vec4::W).norm2() <= 0.001, "Y*Y = -W");
  tassert(((Quat(Vec4::Y) * Quat(Vec4::Z)).v - Vec4::X).norm2() <= 0.001, "Y*Z = +X");
  tassert(((Quat(Vec4::Y) * Quat(Vec4::W)).v - Vec4::Y).norm2() <= 0.001, "Y*W = +Y");

  tassert(((Quat(Vec4::Z) * Quat(Vec4::X)).v - Vec4::Y).norm2() <= 0.001, "Z*X = +Y");
  tassert(((Quat(Vec4::Z) * Quat(Vec4::Y)).v + Vec4::X).norm2() <= 0.001, "Z*Y = -X");
  tassert(((Quat(Vec4::Z) * Quat(Vec4::Z)).v + Vec4::W).norm2() <= 0.001, "Z*Z = -W");
  tassert(((Quat(Vec4::Z) * Quat(Vec4::W)).v - Vec4::Z).norm2() <= 0.001, "Z*W = +Z");

  tassert(((Quat(Vec4::W) * Quat(Vec4::X)).v - Vec4::X).norm2() <= 0.001, "W*X = +X");
  tassert(((Quat(Vec4::W) * Quat(Vec4::Y)).v - Vec4::Y).norm2() <= 0.001, "W*Y = +Y");
  tassert(((Quat(Vec4::W) * Quat(Vec4::Z)).v - Vec4::Z).norm2() <= 0.001, "W*Z = +Z");
  tassert(((Quat(Vec4::W) * Quat(Vec4::W)).v - Vec4::W).norm2() <= 0.001, "W*W = +W");
}

TEST(quatrot) {
  auto quat = Quat::from_axis_angle(Vec4::Y, consts::FRAC_PI_2);
  tassert((quat.rotate(Vec4::X) + Vec4::Z).norm2() <= 0.001);
  tassert((quat.rotate(Vec4::Y) - Vec4::Y).norm2() <= 0.001);
  tassert((quat.rotate(Vec4::Z) - Vec4::X).norm2() <= 0.001);
}
