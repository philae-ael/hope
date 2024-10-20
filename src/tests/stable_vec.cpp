#include "tests.h"

#include <core/containers/stable_vec.h>
#include <core/core.h>

TEST(stable vec) {
  auto scratch = core::scratch_get();

  core::stable_vec<int, 32> sv;
  tassert(sv.capacity() == 0, "invalid capacity");
  sv.push(scratch, 1);
  tassert(sv.capacity() == 2, "invalid capacity");
  sv.push(scratch, 2);
  tassert(sv.capacity() == 2, "invalid capacity");
  sv.push(scratch, 3);
  tassert(sv.capacity() == 4, "invalid capacity");
  tassert(sv.pop() == 3, "pop 3");
  tassert(sv.pop() == 2, "pop 2");
  tassert(sv.pop() == 1, "pop 1");

  tassert(sv.capacity() == 4, "invalid capacity");
  sv.push(scratch, 4);
  tassert(sv.capacity() == 4, "invalid capacity");
  sv.push(scratch, 5);
  tassert(sv.capacity() == 4, "invalid capacity");
  sv.push(scratch, 6);
  tassert(sv.capacity() == 4, "invalid capacity");

  tassert(sv.pop() == 6, "pop 6");
  tassert(sv.pop() == 5, "pop 5");
  tassert(sv.pop() == 4, "pop 4");
}

TEST(stable vec index) {
  auto scratch = core::scratch_get();

  core::stable_vec<int, 32> sv;
  sv.push(scratch, 0);
  sv.push(scratch, 1);
  sv.push(scratch, 2);
  sv.push(scratch, 3);
  sv.push(scratch, 4);
  sv.push(scratch, 5);

  tassert(sv.at(0) == 0, "sv[0] == 0");
  tassert(sv.at(1) == 1, "sv[1] == 1");
  tassert(sv[2] == 2, "sv[2] == 2");
  tassert(sv[3] == 3, "sv[3] == 3");
  tassert(sv[4] == 4, "sv[4] == 4");
  tassert(sv[5] == 5, "sv[5] == 5");

  sv.at(4) = 54;
  tassert(sv.at(4) == 54, "sv[4] == 54");
}

TEST(stable vec iter) {
  auto scratch = core::scratch_get();

  core::stable_vec<int, 32> sv;
  sv.push(scratch, 0);
  sv.push(scratch, 1);
  sv.push(scratch, 2);
  sv.push(scratch, 3);
  sv.push(scratch, 4);
  sv.push(scratch, 5);

  for (auto [i, t] : core::enumerate{sv.iter()}) {
    tassert((int)i == t, "iterate");
    t += 1;
  }

  for (auto [i, t] : core::enumerate{sv.iter()}) {
    tassert(int(i + 1) == t, "iterate2");
  }
}
