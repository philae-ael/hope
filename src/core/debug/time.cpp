#include "time.h"
#include <core/core.h>
#include <core/core/math.h>
#include <core/os/time.h>

struct string_map {
  using T = os::time;
  struct Data {
    core::hstr8 key;
    T value;
  };
  core::vec<Data> data;

  T& operator[](core::hstr8 h) {
    for (usize offset = 0; offset < data.size(); offset++) {
      auto& d = data[offset];
      if (d.key == h) {
        return d.value;
      }
    }
    data.push(core::noalloc, {h, {}});
    return data[data.size() - 1].value;
  }

  void reset() {
    data.set_size(0);
  }

  auto iter() {
    return data.iter();
  }
};

#define DEBUG_MAX_SCOPES 150
#define FRAME_COUNT 100

namespace {
struct {
  string_map::Data storagea[DEBUG_MAX_SCOPES], storageb[DEBUG_MAX_SCOPES];
  string_map last{storagea}, cur{storageb};

  f32 frame_time_storage[FRAME_COUNT];
  core::windowed_series frame_time_series{frame_time_storage};

  os::time frame_start;
} s;
} // namespace

namespace debug {

EXPORT void init() {}
EXPORT void reset() {
  frame_start();
  frame_start();
}

EXPORT void frame_end() {
  s.frame_time_series.add_sample((f32)os::time_monotonic().since(s.frame_start).ns);
}

EXPORT void frame_start() {
  SWAP(s.last, s.cur);
  s.cur.reset();
  s.frame_start = os::time_monotonic();
}

// TODO: move name into a string_registry
EXPORT scope scope_start(core::hstr8 name) {
  return {name, os::time_monotonic()};
}

EXPORT void scope_end(scope sco) {
  s.cur[sco.name] += os::time_monotonic().since(sco.t);
}

EXPORT timing_infos get_last_frame_timing_infos(core::Arena& ar) {
  core::vec<timing_info> v;
  for (auto& scope : s.last.iter()) {
    v.push(ar, {scope.key, scope.value});
  }
  return {
      v,
      {
          {(u64)s.frame_time_series.mean()},
          {(u64)s.frame_time_series.sigma()},
      }
  };
}
} // namespace debug
