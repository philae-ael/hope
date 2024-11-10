#include "tests.h"

TEST(str8) {
  auto a{"A bigger test"_s};
  core::array<u8, 14> b{"a bigger test"};
  TRAP;
  tassert(a == core::str8{1, b.data}, "huhuh");
}
