#include <cstdio>
#include <cstdlib>
#include <version>

#include "core.h"

#if LINUX
  #include <signal.h>
#endif

#ifndef USE_STACKTRACE
  #define USE_STACKTRACE 1
#endif

#if defined(__cpp_lib_stacktrace) && USE_STACKTRACE
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
EXPORT void panic(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fflush(stdout);

  fprintf(stderr, "Panic: ");
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  std::abort();
}

#if !defined(__cpp_lib_stacktrace) || !USE_STACKTRACE
void dump_backtrace_fallback(usize skip) {
  #if LINUX
  // https://gist.github.com/fmela/591333/c64f4eb86037bb237862a8283df70cdfc25f01d3
  void* callstack[128];
  const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
  int nFrames          = backtrace(callstack, nMaxFrames);
  char** symbols       = backtrace_symbols(callstack, nFrames);
  for (usize i = skip; i < (usize)nFrames; i++) {
    Dl_info info;
    if (dladdr(callstack[i], &info)) {
      char* demangled = NULL;
      int status;
      demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
      fprintf(
          stderr, "%3zu %s %s + %ld\n", i - skip, info.dli_fname,
          status == 0 ? demangled : info.dli_sname, (char*)callstack[i] - (char*)info.dli_saddr
      );
      free(demangled);
    } else {
      fprintf(stderr, "%3zu %p\n", i, callstack[i]);
    }
  }
  free(symbols);
  #else
    #error Unsupported system
  #endif
}
#endif

EXPORT void dump_backtrace(usize skip) {
#if defined(__cpp_lib_stacktrace) && USE_STACKTRACE
  std::stacktrace s = std::stacktrace::current();
  for (auto i = (std::stacktrace::size_type)skip; i < s.size(); i++) {
    auto& entry = s[i];
    fprintf(
        stderr, "%3zu %s:%d in %s\n", i - skip, entry.source_file().c_str(), entry.source_line(),
        entry.description().c_str()
    );
  }
#else
  dump_backtrace_fallback(skip + 1);
#endif
}

void crash_handler(int sig) {
  dump_backtrace(2);

  ::signal(sig, SIG_DFL);
  ::raise(sig);
}

EXPORT void setup_crash_handler() {
  ::signal(SIGSEGV, crash_handler);
  ::signal(SIGABRT, crash_handler);
}
} // namespace core
