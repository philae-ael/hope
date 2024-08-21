#include "time.h"
#include "../prelude.h"

namespace os {
duration_info duration_info::from_time(time t) {
  u64 const nanos   = t.ns;
  u64 const micros  = nanos / 1000;
  u64 const millis  = micros / 1000;
  u64 const seconds = millis / 1000;
  u64 const minutes = seconds / 60;
  u64 const hours   = minutes / 60;
  return {
      .hour = static_cast<u16>(hours),
      .min  = static_cast<u16>(minutes % 60),
      .sec  = static_cast<u16>(seconds % 60),
      .msec = static_cast<u16>(millis % 1000),
      .usec = static_cast<u16>(micros % 1000),
      .nsec = static_cast<u16>(nanos % 1000),
  };
}

str8 to_str8(arena& arena, duration_info duration_info, TimeFormat format) {
  core::string_builder sb{};
  switch (format) {
  case TimeFormat::HH_MM_SS_MMM_UUU:
    sb.pushf(
        arena, "%02d:%02d:%02d:%03d.%03d", duration_info.hour, duration_info.min, duration_info.sec,
        duration_info.msec, duration_info.usec
    );
    break;
  case TimeFormat::HH_MM_SS_MMM:
    sb.pushf(
        arena, "%02d:%02d:%02d:%03d", duration_info.hour, duration_info.min, duration_info.sec,
        duration_info.usec
    );
    break;

  case TimeFormat::MM_SS_MMM_UUU_NNN:
    sb.pushf(
        arena, "%02d:%02d:%03d.%03d.%03d", duration_info.min, duration_info.sec, duration_info.msec,
        duration_info.usec, duration_info.nsec
    );
    break;
  case TimeFormat::MM_SS_MMM_UUU:
    sb.pushf(
        arena, "%02d:%02d:%03d.%03d", duration_info.min, duration_info.sec, duration_info.msec,
        duration_info.usec
    );
    break;
  case TimeFormat::MM_SS_MMM:
    sb.pushf(arena, "%02d:%02d:%03d", duration_info.min, duration_info.sec, duration_info.msec);
    break;
  }

  return sb.commit(arena);
}

core::str8 to_str8(core::arena& arena, time t, TimeFormat format) {
  return to_str8(arena, duration_info::from_time(t), format);
}
} // namespace os
