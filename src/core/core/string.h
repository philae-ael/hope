// IWYU pragma: private

#ifndef INCLUDE_CORE_STRING_H_
#define INCLUDE_CORE_STRING_H_
#include <cstdarg>
#include <cstring>

#include "base.h"
#include "fwd.h"
#include "memory.h"
#include "types.h"

namespace core {
// For ADL purpose!
inline str8 to_str8(str8 s) {
  return s;
}
inline str8 to_str8(hstr8 h) {
  return {h.len, h.data};
}

template <size_t len>
str8 to_str8(const char (&a)[len]) {
  return core::str8::from(a);
}

inline str8 to_str8(const char* a) {
  return core::str8::from(a, strlen(a));
}

struct string_node {
  // Care, the order is used in vpushf and is useful for str8::cstring()
  string_node* next;
  str8 str;
};

template <class T>
concept Str8ifiable = requires(T x) {
  { to_str8(x) };
};

template <class T, class... Args>
concept Str8ifiableDyn = requires(Allocator alloc, T x, Args... args) {
  { to_str8(alloc, x, args...) };
} && !Str8ifiable<T>;

struct string_builder {
  string_node* first;
  string_node* last;
  usize total_len;
  usize node_count;

  string_builder& append(string_builder& sb);
  string_builder& push_node(string_node* node);
  string_builder& push_str8(string_node* node, str8 str);
  string_builder& push_str8(Allocator alloc, str8 str);
  template <Str8ifiable T, class... Args>
  string_builder& push(Allocator alloc, T&& t, Args&&... args) {
    return push_str8(alloc, to_str8(FWD(t), FWD(args)...));
  }
  template <class T, class... Args>
  string_builder& push(Allocator alloc, T&& t, Args&&... args)
    requires Str8ifiableDyn<T, Args...>
  {
    return push_str8(alloc, to_str8(alloc, FWD(t), FWD(args)...));
  }

  PRINTF_ATTRIBUTE(3, 4) string_builder& pushf(Allocator alloc, const char* fmt, ...);
  string_builder& vpushf(Allocator alloc, const char* fmt, va_list ap);
  str8 commit(Allocator alloc, str8 join = {}) const;
};

template <class... Args>
str8 join(core::Allocator alloc, str8 sep, const Args&... args) {
  auto scratch = scratch_get();
  string_builder sb{};
  (sb.push(scratch, args), ...);
  return sb.commit(alloc, sep);
}

namespace literals {
inline str8 operator""_s(const char* s, std::size_t len) {
  return str8::from(s, len);
}

inline hstr8 operator""_hs(const char* s, std::size_t len) {
  return {core::hash(s, len), len, reinterpret_cast<const u8*>(s)};
}
constexpr inline u64 operator""_h(const char* s, std::size_t len) {
  return hash(s, len);
}
} // namespace literals

template <class T>
str8 to_str8(Allocator alloc, Maybe<T> m)
  requires Str8ifiable<T> || Str8ifiableDyn<T>
{
  using namespace literals;
  if (m.is_some()) {
    return string_builder{}.push(alloc, "Some(").push(alloc, m.value()).push(alloc, ")").commit(alloc);
  } else {
    return "None"_s;
  }
}

enum class Os { Windows, Linux };
str8 to_str8(Os os);

struct split : cpp_iter<str8, split> {
  str8 rest;
  const char c;
  split(str8 s, char sep)
      : rest(s)
      , c(sep) {}

  Maybe<str8> next() {
    if (rest.len == 0) {
      return {};
    }
    usize i = 0;
    while (i < rest.len && rest[i] != c)
      i++;

    auto ret = rest.subslice(0, i);
    rest     = rest.subslice(i);
    if (rest.len > 0)
      rest = rest.subslice(1);
    return ret;
  };
};

hstr8 intern(hstr8);
hstr8 unintern(u64 hash);

} // namespace core
#endif // INCLUDE_CORE
