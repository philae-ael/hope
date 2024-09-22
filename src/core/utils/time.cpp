#include "time.h"
#include <core/core.h>
#include <core/math.h>
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

#define MAX_SCOPES 150
#define FRAME_COUNT 200

static string_map::Data storagea[2][MAX_SCOPES], storageb[2][MAX_SCOPES];

static core::array lasts{
    string_map{storagea[0]},
    string_map{storagea[1]},
};

static core::array curs{
    string_map{storageb[0]},
    string_map{storageb[1]},
};

static f32 frame_time_storage[FRAME_COUNT];
static math::windowed_series frame_time_series{frame_time_storage};

static os::time frame_start_t;

namespace utils {

EXPORT void timings_init() {
  lasts[0].reset();
  curs[0].reset();
  lasts[1].reset();
  curs[1].reset();
}
EXPORT void timings_reset() {}

EXPORT void timings_frame_end() {
  f32 sample = (f32)os::time_monotonic().since(frame_start_t).ns;
  frame_time_series.add_sample(sample);
}

EXPORT void timings_frame_start(scope_category cat) {
  auto& cur  = curs[(usize)cat];
  auto& last = lasts[(usize)cat];

  SWAP(last, cur);
  cur.reset();

  if (cat == scope_category::CPU) {
    frame_start_t = os::time_monotonic();
  }
}

// TODO: move name into a string_registry
EXPORT scope scope_start(core::hstr8 name) {
  return {core::intern(name), os::time_monotonic()};
}

EXPORT void scope_end(scope sco) {
  scope_import(scope_category::CPU, sco.name, os::time_monotonic().since(sco.t));
}

EXPORT void scope_import(scope_category cat, core::hstr8 name, os::time t) {
  auto& cur                = curs[(usize)cat];
  cur[core::intern(name)] += t;
}

EXPORT timing_infos get_last_frame_timing_infos(core::Allocator alloc, scope_category cat) {
  auto& last = lasts[(usize)cat];
  core::vec<timing_info> v;

  for (auto& scope : last.iter()) {
    v.push(alloc, {scope.key, scope.value});
  }
  return {
      v,
      {
          {(u64)frame_time_series.last_sample()},
          {(u64)frame_time_series.mean()},
          {(u64)frame_time_series.sigma()},
          {(u64)frame_time_series.low_95()},
          {(u64)frame_time_series.low_99()},
      }
  };
}

EXPORT os::time get_last_frame_dt() {
  return os::time{(u64)frame_time_series.mean()};
}
} // namespace utils
