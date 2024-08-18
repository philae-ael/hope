// IWYU pragma: private, include "core/core.h"

#ifndef INCLUDE_CORE_STRING_H_
#define INCLUDE_CORE_STRING_H_
#include <cstdarg>

#include "memory.h"
#include "platform.h"
#include "types.h"

namespace core {

struct hstr8 {
  u64 hash;
  usize len;
  const u8 *data;
};

constexpr u64 hash(const u8 *data, size_t len) {
  // implementation of fnv1
  u64 h = 0xcbf29ce484222325;
  for (usize i = 0; i < len; i++) {
    h *= 0x100000001b3;
    h ^= u64(data[i]);
  }
  return h;
}
constexpr u64 hash(const char *data, size_t len) {
  // implementation of fnv1
  u64 h = 0xcbf29ce484222325;
  for (usize i = 0; i < len; i++) {
    h *= 0x100000001b3;
    h ^= u64(data[i]);
  }
  return h;
}

struct str8 {
  usize len;
  const u8 *data;

  template <size_t len> static str8 from(const char (&d)[len]) {
    return from(d, len - 1);
  }
  static str8 from(const char *d, size_t len) {
    return str8{len, reinterpret_cast<const u8 *>(d)};
  }
  hstr8 hash() const {
    return {
        ::core::hash(data, len),
        len,
        data,
    };
  }

  const char *cstring(Arena &arena);
};

// For ADL purpose!
inline str8 to_str8(str8 s) { return s; }

template <size_t len> str8 to_str8(const char (&a)[len]) {
  return core::str8::from(a);
}

struct string_node {
  string_node *next;
  str8 str;
};

template <class T>
concept Str8ifiable = requires(T x) {
  { to_str8(x) };
};

template <class T>
concept Str8ifiableDyn = requires(Arena &arena, T x) {
  { to_str8(arena, x) };
} && !Str8ifiable<T>;

struct string_builder {
  string_node *first;
  string_node *last;
  usize total_len;

  string_builder &append(string_builder &sb);
  string_builder &push_node(string_node *node);
  string_builder &push_str8(string_node *node, str8 str);
  string_builder &push_str8(Arena &arena, str8 str);
  template <Str8ifiable T, class... Args>
  string_builder &push(Arena &arena, T &&t, Args &&...args) {
    return push_str8(arena, to_str8(FWD(t), FWD(args)...));
  }
  template <Str8ifiableDyn T, class... Args>
  string_builder &push(Arena &arena, T &&t, Args &&...args) {
    return push_str8(arena, to_str8(arena, FWD(t), FWD(args)...));
  }

  PRINTF_ATTRIBUTE(3, 4)
  string_builder &pushf(Arena &arena, const char *fmt, ...);
  string_builder &vpushf(Arena &arena, const char *fmt, va_list ap);
  str8 commit(Arena &arena) const;
};

namespace literals {
inline str8 operator""_s(const char *s, std::size_t len) {
  return str8::from(s, len);
}

inline hstr8 operator""_hs(const char *s, std::size_t len) {
  return str8::from(s, len).hash();
}
constexpr inline u64 operator""_h(const char *s, std::size_t len) {
  return hash(s, len);
}
} // namespace literals

template <class T>
str8 to_str8(Arena &arena, Maybe<T> m)
  requires Str8ifiable<T> || Str8ifiableDyn<T>
{
  using namespace literals;
  if (m.is_some()) {
    return string_builder{}
        .push(arena, "Some(")
        .push(arena, m.value())
        .push(arena, ")")
        .commit(arena);
  } else {
    return "None"_s;
  }
}

enum class Os { Windows, Linux };
str8 to_str8(Os os);
template <> Maybe<Os> from_hstr8<Os>(hstr8 h);


} // namespace core
#endif // INCLUDE_CORE_STRING_H_
