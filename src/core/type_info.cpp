#include "core.h"

core::str8 core::to_str8(arena& ar, layout_info l) {
  string_builder sb{};
  sb.pushf(ar, "Layout{.size=%zu, alignement=%zu}", l.size, l.alignement);
  return sb.commit(ar);
}
