#include "type_info.h"
#include "string.h"

core::str8 core::to_str8(Arena& ar, LayoutInfo l) {
  string_builder sb{};
  sb.pushf(ar, "Layout{.size=%zu, alignement=%zu}", l.size, l.alignement);
  return sb.commit(ar);
}
