#include "string.h"
#include "core.h"
#include <cstdio>
#include <cstring>

namespace core {

string_builder& string_builder::pushf(arena& arena, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vpushf(arena, fmt, ap);
  va_end(ap);
  return *this;
}

string_builder& string_builder::vpushf(arena& arena, const char* fmt, va_list ap) {
  va_list ap_copy;

  va_copy(ap_copy, ap);
  usize len = vsnprintf(nullptr, 0, fmt, ap_copy);
  va_end(ap_copy);

  string_node* n = (string_node*)arena.allocate(
      sizeof(string_node) + len + 1, alignof(string_node),
      "string_buffer::vpushf"
  ); // vsnprintf writes a \0 at the end!
  n->str.len      = len;
  n->str.data     = (u8*)n + sizeof(string_node);

  usize len_wrote = vsnprintf((char*)n->str.data, len + 1, fmt, ap);
  ASSERT(len_wrote == len);

  push_node(n);
  return *this;
}

str8 string_builder::commit(arena& arena) const {
  if (first != nullptr && first->next == nullptr && arena.owns((void*)first->str.data)) {
    return first->str;
  }

  const string_node* node = first;
  u8* data                = (u8*)arena.allocate(total_len);

  usize offset            = 0;

  while (node != nullptr) {
    memcpy(data + offset, node->str.data, node->str.len);
    offset += node->str.len;
    node    = node->next;
  }

  ASSERT(offset == total_len);

  return str8{total_len, data};
}

string_builder& string_builder::push_str8(arena& arena, str8 str) {
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

string_builder& string_builder::push_node(string_node* node) {
  if (last != nullptr) {
    last->next = node;
    last       = node;
  } else {
    first = last = node;
  }

  total_len += node->str.len;
  return *this;
}

string_builder& string_builder::append(string_builder& sb) {
  if (sb.first == nullptr) {
    return *this;
  }

  if (last != nullptr) {
    last->next  = sb.first;
    last        = sb.last;
    total_len  += sb.total_len;
  } else {
    *this = sb;
  }
  return *this;
}

const char* str8::cstring(arena& arena) {
  u8* cstr = (u8*)data;
  if (!arena.try_resize((void*)data, len, len + 1)) {
    cstr = (u8*)arena.allocate(len + 1, alignof(char));
    memcpy(cstr, data, len);
  }

  cstr[len] = 0;
  return (const char*)cstr;
}

} // namespace core
