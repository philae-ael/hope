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

struct timing_infos {
  core::vec<timing_info> timings;
  struct {
    os::time raw_frame_time;
    os::time mean_frame_time;
    os::time sigma_frame_time;
    os::time low_95;
    os::time low_99;
  } stats;
};

enum class scope_category { CPU, GPU };

struct scope {
  core::hstr8 name;
  os::time t;
};

void init();
void reset();

void frame_start(scope_category cat = scope_category::CPU);
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
void scope_import(scope_category cat, core::hstr8 name, os::time t);

timing_infos get_last_frame_timing_infos(core::Arena& ar, scope_category cat = scope_category::CPU);
os::time get_last_frame_dt();
} // namespace debug

#endif // INCLUDE_DEBUG_TIME_H_
