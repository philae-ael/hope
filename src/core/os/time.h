// IWYU pragma: private

#ifndef INCLUDE_OS_TIME_H_
#define INCLUDE_OS_TIME_H_

#include "../core/fwd.h"

namespace core {
struct Arena;
} // namespace core

namespace os {
struct time {
  u64 ns; // 580 years, should be enough

  time since(const time& other) const {
    return {ns - other.ns};
  }
  f64 timestampf64() const {
    return static_cast<f64>(ns) / 1e9F;
  }
  time operator+(time other) const {
    return {ns + other.ns};
  }

  time& operator+=(time other) {
    ns += other.ns;
    return *this;
  }

  f32 hz() const {
    return 1e+9f / (f32)ns;
  }
};

struct duration_info {
  u16 hour;
  u16 min;
  u16 sec;
  u16 msec;
  u16 usec;
  u16 nsec;
  static duration_info from_time(time t);
};

duration_info time_realtime();

time time_monotonic();
time time_monotonic_resolution();

enum class TimeFormat {
  HH_MM_SS_MMM_UUU,
  HH_MM_SS_MMM,
  MM_SS_MMM_UUU_NNN,
  MM_SS_MMM_UUU,
  MM_SS_MMM,
  MMM_UUU_NNN,
};

core::str8 to_str8(
    core::Arena& arena,
    duration_info duration_info,
    TimeFormat format = TimeFormat::HH_MM_SS_MMM
);
core::str8 to_str8(core::Arena& arena, time t, TimeFormat format = TimeFormat::MM_SS_MMM_UUU);
} // namespace os
#endif // INCLUDE_OS_TIME_H_
