#include "string.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <core/core.h>
#include <unordered_map>

namespace core {

EXPORT string_builder& string_builder::pushf(Arena& arena, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vpushf(arena, fmt, ap);
  va_end(ap);
  return *this;
}

EXPORT string_builder& string_builder::vpushf(Arena& arena, const char* fmt, va_list ap) {
  va_list ap_copy;

  va_copy(ap_copy, ap);
  usize len = (usize)vsnprintf(nullptr, 0, fmt, ap_copy);
  va_end(ap_copy);

  string_node* n = (string_node*)arena.allocate(
      sizeof(string_node) + len + 1, alignof(string_node),
      "string_buffer::vpushf"
  ); // vsnprintf writes a \0 at the end!
  n->str.len      = len;
  n->str.data     = (u8*)n + sizeof(string_node);

  usize len_wrote = (usize)vsnprintf((char*)n->str.data, len + 1, fmt, ap);
  ASSERT(len_wrote == len);

  push_node(n);
  return *this;
}

EXPORT str8 string_builder::commit(Arena& arena, str8 join) const {
  if (first != nullptr && first->next == nullptr && arena.owns((void*)first->str.data)) {
    return first->str;
  }
  DEBUG_ASSERT(node_count >= 1);

  const usize expected_len = total_len + (node_count - 1) * join.len;
  const string_node* node  = first;
  u8* data                 = (u8*)arena.allocate(expected_len);
  usize offset             = 0;
  usize count              = 0;
  while (node != nullptr) {
    memcpy(data + offset, node->str.data, node->str.len);
    offset += node->str.len;
    if (node->next != nullptr) {
      memcpy(data + offset, join.data, join.len);
      offset += join.len;
    }
    node = node->next;
    count++;
  }

  ASSERT(offset == expected_len);
  ASSERT(count == node_count);

  return str8{expected_len, data};
}

EXPORT string_builder& string_builder::push_str8(Arena& arena, str8 str) {
  return push_str8(arena.allocate<string_node>(), str);
}

string_builder& string_builder::push_str8(string_node* uninit_node, str8 str) {
  if (str.len == 0) {
    return *this;
  }

  uninit_node->next = 0;
  uninit_node->str  = str;
  push_node(uninit_node);
  return *this;
}

EXPORT string_builder& string_builder::push_node(string_node* node) {
  if (last != nullptr) {
    last->next = node;
    last       = node;
  } else {
    first = last = node;
  }

  total_len  += node->str.len;
  node_count += 1;
  return *this;
}

EXPORT string_builder& string_builder::append(string_builder& sb) {
  if (sb.first == nullptr) {
    return *this;
  }

  if (last != nullptr) {
    last->next  = sb.first;
    last        = sb.last;
    total_len  += sb.total_len;
    node_count += sb.node_count;
  } else {
    *this = sb;
  }
  return *this;
}

EXPORT str8 str8::clone(Arena& arena) {
  auto storage = arena.allocate_array<u8>(len);
  memcpy(storage.data, data, len);
  return {len, storage.data};
}

EXPORT hstr8 hstr8::clone(Arena& arena) {
  auto s = to_str8(*this).clone(arena);
  return {hash, s.len, s.data};
}

EXPORT const char* str8::cstring(Arena& arena) {
  u8* cstr = (u8*)data;
  if (!arena.try_resize((void*)data, len, len + 1)) {
    cstr = (u8*)arena.allocate(len + 1, alignof(char));
    memcpy(cstr, data, len);
  }

  cstr[len] = 0;
  return (const char*)cstr;
}

struct {
  std::unordered_map<u64, hstr8> m;
  Arena* arena = nullptr;
} interned;

EXPORT hstr8 intern(hstr8 s) {
  if (interned.arena == nullptr) {
    interned.arena = &arena_alloc();
  }

  auto it = interned.m.find(s.hash);
  if (it == interned.m.end()) {
    auto h = s.clone(*interned.arena);
    interned.m.insert({h.hash, h});
    return h;
  }
  return it->second;
}

EXPORT hstr8 unintern(u64 hash) {
  auto it = interned.m.find(hash);
  if (it == interned.m.end()) {
    LOG_WARNING("trying to unintern an unknown string");
    TRAP;
    using namespace core::literals;
    return "<unknown>"_hs;
  }

  return it->second;
}

} // namespace core
