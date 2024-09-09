#include "time.h"
#include "core/core.h"
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

namespace {
struct {
  string_map::Data storagea[DEBUG_MAX_SCOPES], storageb[DEBUG_MAX_SCOPES];
  string_map last{storagea}, cur{storageb};
} s;
} // namespace

namespace debug {

EXPORT void init() {}

EXPORT void frame_end() {}

EXPORT void frame_start() {
  SWAP(s.last, s.cur);
  s.cur.reset();
}

// TODO: move name into a string_registry
EXPORT scope scope_start(core::hstr8 name) {
  return {name, os::time_monotonic()};
}

EXPORT void scope_end(scope sco) {
  s.cur[sco.name] += os::time_monotonic().since(sco.t);
}

EXPORT core::vec<timing_info> get_last_frame_timing_infos(core::Arena& ar) {
  core::vec<timing_info> v;
  for (auto& scope : s.last.iter()) {
    v.push(ar, {scope.key, scope.value});
  }
  return v;
}
} // namespace debug
