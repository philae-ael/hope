#include "tests.h"

TEST(str8) {
  auto a{"A bigger test"_s};
  core::array<u8, 14> b{"A bigger test"};
  tassert(a == core::str8{14, b.data}, "huhuh");
}
