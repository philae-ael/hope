#include "tests.h"

#include <core/core.h>
#include <utility>

TEST(tuple set) {
  core::tuple<int, int, int, u8> a{0, 1, 2, 8};
  core::get<3>(a) = 5;
}

TEST(tuple ptr) {
  struct A {};
  const core::tuple<A, int, int> a{{}, 0, 1};
  tassert((u8*)&get<0>(a) == (u8*)&get<1>(a), "heu");
  tassert((u8*)&get<1>(a) + sizeof(int) == (u8*)&get<2>(a), "heu");
}
TEST(tuple get by type) {
  core::tuple<int, usize, char, u8> a{0, 1, 2, 8};

  get_by_type<int>(a) = 78;
  tassert(get_by_type<int>(std::as_const(a)) == 78, "get/set by type");

  get_by_type<u8>(a) = 68;
  tassert(get_by_type<u8>(std::as_const(a)) == 68, "get/set by type");
}

TEST(tuple to struct) {
  struct A {
    u16 a;
    char b;
    u32 c;
  };
  const core::tuple<u16, char, u32> a{0, 1, 2};
  A aa = core::map_construct<A>(a, core::identity_f);
  tassert(aa.a == 0, "a == 0");
  tassert(aa.b == 1, "b == 1");
  tassert(aa.c == 2, "c == 2");
}
