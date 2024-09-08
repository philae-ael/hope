#include "time.h"
#include <core/os/time.h>

namespace {
struct {
  core::Arena *last_ar, *cur_ar;
  debug::timing_infos last, cur;

  os::time frame_start;
} s;
} // namespace

namespace debug {

EXPORT void init() {
  s.last_ar = &core::arena_alloc(KB(4));
  s.cur_ar  = &core::arena_alloc(KB(4));
}

EXPORT void frame_end() {}

EXPORT void frame_start() {
  SWAP(s.last, s.cur);
  SWAP(s.last_ar, s.cur_ar);
  s.cur.timings = {};
  s.cur_ar->reset();
  s.frame_start = os::time_monotonic();
}

// TODO: move name into a string_registry
EXPORT void add_timestamp(core::hstr8 name) {
  s.cur.timings.push(
      *s.cur_ar,
      {
          name,
          os::time_monotonic().since(s.frame_start),
      }
  );
}

EXPORT const timing_infos& get_last_frame_timing_infos() {
  return s.last;
}
} // namespace debug
