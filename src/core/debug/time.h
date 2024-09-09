#ifndef INCLUDE_DEBUG_TIME_H_
#define INCLUDE_DEBUG_TIME_H_

#include <core/containers/vec.h>
#include <core/core.h>
#include <core/os/time.h>

namespace debug {

struct timing_info {
  core::hstr8 name;
  os::time time;
};
struct scope {
  core::hstr8 name;
  os::time t;
};

void init();

void frame_start();
void frame_end();

// Note: This is not recursion compatible:
// Scope A:
// do things (for t1)
// Scope A (2)
// do things (for t2)
// End Scope A (2)
// End Scope A (1)
//
// Time registered will be t1 + 2*t2 and not t1 + t2
scope scope_start(core::hstr8 name);
void scope_end(scope);

core::vec<timing_info> get_last_frame_timing_infos(core::Arena& ar);
} // namespace debug

#endif // INCLUDE_DEBUG_TIME_H_
