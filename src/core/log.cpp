#include "log.h"
#include "core.h"
#include "debug.h"
#include "fwd.h"
#include <cstdarg>
#include <cstdio>
#include <source_location>

using namespace core::literals;
namespace core {

/* FORMATTERS */

log_entry default_log_formatter(void*, Arena& arena, log_entry entry) {
  entry.builder = string_builder{}
                      .push(arena, entry.level)
                      .push(arena, ": ")
                      .append(entry.builder)
                      .push(arena, "\n");
  return entry;
}

#define ESCAPE "\x1B"
static core::str8 LEVEL_COLOR[]{
    ""_s,            // Trace,
    ""_s,            // Debug,
    ESCAPE "[34m"_s, // Info, blue
    ESCAPE "[33m"_s, // Warning, yellow
    ESCAPE "[31m"_s, // Error, red
};

static core::str8 COLOR_RESET = ESCAPE "[0m"_s;

log_entry log_fancy_formatter(void*, core::Arena& arena, core::log_entry entry) {
  entry.builder = core::string_builder{}
                      .pushf(arena, "%s:%d - ", entry.loc.file_name(), entry.loc.line())
                      .push(arena, LEVEL_COLOR[(usize)entry.level])
                      .push(arena, entry.level)
                      .push(arena, COLOR_RESET)
                      .push(arena, ": ")
                      .append(entry.builder)
                      .push(arena, "\n");
  return entry;
}

void default_log_writer(void*, str8 msg) {
  fwrite(msg.data, 1, msg.len, stdout);
}

/* REGISTRATION */

static log_writer global_log_writer        = default_log_writer;
static void* global_log_writer_userdata    = nullptr;
static log_formatter global_log_formatter  = default_log_formatter;
static void* global_log_formatter_userdata = nullptr;
static LogLevel global_log_level           = LOG_DEFAULT_GLOBAL_LEVEL;

void log_register_global_writer(log_writer w, void* user) {
  global_log_writer          = w;
  global_log_writer_userdata = user;
}
void log_register_global_formatter(log_formatter f, void* user) {
  global_log_formatter          = f;
  global_log_formatter_userdata = user;
}

void log_set_global_level(LogLevel level) {
  global_log_level = level;
}
bool log_filter(LogLevel level) {
  return (usize)level >= (usize)global_log_level;
}

void log_emit(Arena& arena, log_entry& entry) {
  str8 msg =
      global_log_formatter(global_log_formatter_userdata, arena, entry).builder.commit(arena);
  global_log_writer(global_log_writer_userdata, msg);
}

/* LogLevel */

str8 to_str8(LogLevel level) {
  return (str8[]){
      "trace"_s, "debug"_s, "info"_s, "warning"_s, "error"_s,
  }[(usize)level];
}

template <>
Maybe<LogLevel> from_hstr8(core::hstr8 s) {
  switch (s.hash) {
  case "trace"_h:
    return LogLevel::Trace;
  case "debug"_h:
    return LogLevel::Debug;
  case "info"_h:
    return LogLevel::Info;
  case "warning"_h:
    return LogLevel::Warning;
  case "error"_h:
    return LogLevel::Error;
  }
  return {};
}

/* LogBuilder */

log_builder& log_builder::pushf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vpushf(fmt, ap);
  va_end(ap);
  return *this;
}
log_builder& log_builder::vpushf(const char* fmt, va_list ap) {
  entry.builder.vpushf(*arena, fmt, ap);
  return *this;
}

log_builder& log_builder::push_str8(str8 msg) {
  entry.builder.push_str8(*arena, msg);
  return *this;
}

using namespace core::enum_helpers;

void log_builder::emit() {
  log_emit(*arena, entry);

  if (any(flags & flags_t::stacktrace)) {
    dump_backtrace(2);
  }
  scratch_retire(arena);
  if (any(flags & flags_t::panic)) {
    ::core::panic("can't continue");
  }
}

log_builder::log_builder(LogLevel level, std::source_location loc)
    : arena(scratch_get())
    , entry{.level = level, .loc = loc} {}

log_builder& log_builder::with_stacktrace() {
  flags = flags | flags_t::stacktrace;
  return *this;
}
log_builder& log_builder::panic() {
  flags = flags | flags_t::panic;
  return *this;
}

} // namespace core
