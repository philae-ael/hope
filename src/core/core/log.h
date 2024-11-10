// IWYU pragma: private

#ifndef INCLUDE_CORE_LOG_H_
#define INCLUDE_CORE_LOG_H_

#include <cstdarg>
#include <utility>

#include "memory.h"
#include "string.h"

#define LOG_DEFAULT_GLOBAL_LEVEL ::core::LogLevel::Trace

namespace core {

template <class... Args>
struct cformat : tuple<Args...> {};

template <class... Args>
cformat(Args...) -> cformat<Args...>;

template <class... Args>
struct format : tuple<Args...> {};

template <class... Args>
format(Args...) -> format<Args...>;

enum class LogLevel {
  Trace,
  Debug,
  Info,
  Warning,
  Error,
};

str8 to_str8(LogLevel level);

struct log_entry {
  LogLevel level;
  string_builder builder;
  source_location loc;
  str8 sep;
};

using log_formatter = log_entry (*)(void*, Allocator, log_entry);
void log_register_global_formatter(log_formatter, void* user);

using log_writer = void (*)(void*, str8);

void log_register_global_writer(log_writer, void* user);
void log_emit(Arena&, log_entry&);
bool log_filter(LogLevel level);
void log_set_global_level(LogLevel level);
LogLevel log_get_global_level();

struct log_builder {
  Scratch arena;
  log_entry entry;

  enum class flags_t {
    stacktrace = 0x1,
    panic      = 0x2,
  } flags = {};

  template <class T, class... Args>
  log_builder& push(T&& t, Args&&... args) {
    entry.builder.push(arena, FWD(t), FWD(args)...);
    return *this;
  }
  template <class... Args>
  log_builder& push(format<Args...>&& format) {
    auto do_ = [&format, this]<usize... Idx>(std::index_sequence<Idx...>) {
      (void)this;
      push(static_cast<Args&&>(get<Idx>(format))...);
    };
    do_(std::index_sequence_for<Args...>());
    return *this;
  }

  template <class... Args>
  log_builder& push_many(Args&&... args) {
    return (push(FWD(args)), ...);
  }

  PRINTF_ATTRIBUTE(2, 3) log_builder& pushf(const char* fmt, ...);
  log_builder& vpushf(const char* fmt, va_list ap);

  template <class... Args>
  log_builder& push(cformat<Args...>&& format) {
    auto do_ = [&format, this]<usize... Idx>(std::index_sequence<Idx...>) {
      pushf(get<Idx>(format)...);
    };
    do_(std::index_sequence_for<Args...>());
    return *this;
  }
  log_builder& push_str8(str8 msg);

  log_builder& with_stacktrace();
  log_builder& panic();
  log_builder& sep(str8 sep) {
    entry.sep = sep;
    return *this;
  }

  void emit();

  log_builder(LogLevel level, source_location loc);
};

log_entry log_fancy_formatter(void*, Allocator alloc, core::log_entry entry);
log_entry log_timed_formatter(void*, Allocator alloc, core::log_entry entry);

#define LOG_BUILDER(level, instr) \
  (log_filter(level) ? core::log_builder(level, CURRENT_SOURCE_LOCATION).instr.emit() : (void)0)

#define LOG(level, fmt, ...) LOG_BUILDER(level, pushf(fmt __VA_OPT__(, __VA_ARGS__)))
#define LOG_DEBUG(fmt, ...) LOG(::core::LogLevel::Debug, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG_INFO(fmt, ...) LOG(::core::LogLevel::Info, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG_TRACE(fmt, ...) LOG(::core::LogLevel::Trace, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG_WARNING(fmt, ...) LOG(::core::LogLevel::Warning, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG_ERROR(fmt, ...) LOG(::core::LogLevel::Error, fmt __VA_OPT__(, __VA_ARGS__))

#define LOG2(level, fmt, ...) LOG_BUILDER(level, push_many(fmt __VA_OPT__(, __VA_ARGS__)))
#define LOG2_DEBUG(fmt, ...) LOG2(::core::LogLevel::Debug, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG2_INFO(fmt, ...) LOG2(::core::LogLevel::Info, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG2_TRACE(fmt, ...) LOG2(::core::LogLevel::Trace, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG2_WARNING(fmt, ...) LOG2(::core::LogLevel::Warning, fmt __VA_OPT__(, __VA_ARGS__))
#define LOG2_ERROR(fmt, ...) LOG2(::core::LogLevel::Error, fmt __VA_OPT__(, __VA_ARGS__))
} // namespace core

#endif // INCLUDE_CORE_LOG_H_
