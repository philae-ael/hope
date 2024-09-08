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
};
void init();

void frame_start();
void frame_end();

void add_timestamp(core::hstr8 name);

const timing_infos& get_last_frame_timing_infos();
} // namespace debug

#endif // INCLUDE_DEBUG_TIME_H_
