#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <source_location>

#include <version>

#include "core.h"

#ifdef __cpp_lib_stacktrace
#include <stacktrace>
#else
#if LINUX
#include <cxxabi.h>   // for __cxa_demangle
#include <dlfcn.h>    // for dladdr
#include <execinfo.h> // for backtrace
#else
#error Unsupported system
#endif
#endif

namespace core {
void panic(const char *msg, std::source_location loc, ...) {
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "Panic in %s:%d: ", loc.file_name(), loc.line());
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  std::abort();
}

#ifndef __cpp_lib_stacktrace
void dump_backtrace_fallback(int skip) {
#if LINUX
  // https://gist.github.com/fmela/591333/c64f4eb86037bb237862a8283df70cdfc25f01d3
  void *callstack[128];
  const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
  int nFrames = backtrace(callstack, nMaxFrames);
  char **symbols = backtrace_symbols(callstack, nFrames);
  for (int i = skip; i < nFrames; i++) {
    Dl_info info;
    if (dladdr(callstack[i], &info)) {
      char *demangled = NULL;
      int status;
      demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
      fprintf(stderr, "%3d %s %s + %ld\n", i - skip, info.dli_fname,
              status == 0 ? demangled : info.dli_sname,
              (char *)callstack[i] - (char *)info.dli_saddr);
      free(demangled);
    } else {
      fprintf(stderr, "%3d %p\n", i, callstack[i]);
    }
  }
  free(symbols);
#else
#error Unsupported system
#endif
}
#endif

void dump_backtrace(int skip) {
#ifdef __cpp_lib_stacktrace
  std::stacktrace s = std::stacktrace::current();
  for (usize i = skip; i < s.size(); i++) {
    auto &entry = s[i];
    fprintf(stderr, "%3zu %s:%d in %s\n", i - skip, entry.source_file().c_str(),
            entry.source_line(), entry.description().c_str());
  }
#else
  dump_backtrace_fallback(skip + 1);
#endif
}

} // namespace core
