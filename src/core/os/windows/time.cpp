#include "../time.h"
#include <core/core.h>
#include <ctime>

#include <chrono>
#include <thread>
#include <windows.h>

namespace os {
time from_timespec_raw(timespec tp) {
  return time{
      .ns = static_cast<u64>(tp.tv_sec) * 1000000000L + static_cast<u64>(tp.tv_nsec),
  };
}

const os::time resolution = [] {
  unsigned __int64 freq;
  QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
  f64 timerResolution = (1.0e9 / (f64)freq);
  return os::time{u64(timerResolution)};
}();

time get_boottime() {
  unsigned __int64 freq;
  QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
  f64 timerResolution = (1.0e9 / (f64)freq);

  unsigned __int64 startTime;
  QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
  return {u64(startTime) * u64(timerResolution)};
}
const time boottime = get_boottime();

EXPORT time time_monotonic_resolution() {
  return resolution;
}

EXPORT time time_monotonic() {
  unsigned __int64 startTime;
  QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
  return os::time{u64(startTime) * resolution.ns}.since(boottime);
}

EXPORT duration_info time_realtime() {
  time_t t{};
  ::time(&t);

  struct tm tm;
  localtime_s(&tm, &t);
  return duration_info{
      .hour = static_cast<u16>(tm.tm_hour),
      .min  = static_cast<u16>(tm.tm_min),
      .sec  = static_cast<u16>(tm.tm_sec),
      .msec = 0,
      .usec = 0,
      .nsec = 0,
  };
}

EXPORT void sleep(u64 ns) {
  std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}
} // namespace os
