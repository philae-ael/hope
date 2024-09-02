#include "../time.h"
#include <core/core.h>
#include <ctime>
#include <sys/time.h>

// POSIX ensure that time_t is an integer (since epoch)
#include <unistd.h>
#ifndef _POSIX_VERSION
  #error LINUX NOT POSIX! (?!!!!!)
#endif // !_POSIX_VERSION

namespace os {
time from_timespec_raw(timespec tp) {
  return time{
      .ns = static_cast<u64>(tp.tv_sec) * 1000000000L + static_cast<u64>(tp.tv_nsec),
  };
}

EXPORT time get_boottime() {
  struct timespec tp {};
  ::clock_gettime(CLOCK_MONOTONIC, &tp);
  return from_timespec_raw(tp);
}
const time boottime = get_boottime();

EXPORT time time_monotonic_resolution() {
  struct timespec tp;
  ::clock_getres(CLOCK_MONOTONIC, &tp);
  return from_timespec_raw(tp);
}

EXPORT time time_monotonic() {
  struct timespec tp;
  ::clock_gettime(CLOCK_MONOTONIC, &tp);
  return from_timespec_raw(tp).since(boottime);
}

EXPORT duration_info time_realtime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm tm;
  localtime_r(&tv.tv_sec, &tm);
  return duration_info{
      .hour = static_cast<u16>(tm.tm_hour),
      .min  = static_cast<u16>(tm.tm_min),
      .sec  = static_cast<u16>(tm.tm_sec),
      .msec = static_cast<u16>(tv.tv_usec / 1000),
      .usec = static_cast<u16>(tv.tv_usec % 1000),
      .nsec = 0,

  };
}

} // namespace os
