#include "math.h"

#include <core/core.h>

namespace core {
struct Arena;
} // namespace core

const char* VEC2_FMT_FLAGS[]{
    "{%*.*g, %*.*g}", "{%-*.*g, %-*.*g}", "{\n  %*.*g,\n  %*.*g,\n}", "{\n  %-*.*g,\n  %-*.*g,\n}",
    "{%*.*f, %*.*f}", "{%-*.*f, %-*.*f}", "{\n  %*.*f,\n  %*.*f,\n}", "{\n  %-*.*f,\n  %-*.*f,\n}",
};

const char* VEC4_FMT_FLAGS[]{
    "{%*.*g, %*.*g, %*.*g, %*.*g}",
    "{%-*.*g, %-*.*g, %-*.*g, %-*.*g}",
    "{\n  %*.*g,\n  %*.*g,\n  %*.*g,\n  %*.*g,\n}",
    "{\n  %-*.*g,\n  %-*.*g,\n  %-*.*g,\n  %-*.*g,\n}",
    "{%*.*f, %*.*f, %*.*f, %*.*f}",
    "{%-*.*f, %-*.*f, %-*.*f, %-*.*f}",
    "{\n  %*.*f,\n  %*.*f,\n  %*.*f,\n  %*.*f,\n}",
    "{\n  %-*.*f,\n  %-*.*f,\n  %-*.*f,\n  %-*.*f,\n}",
};

namespace math {
EXPORT core::str8 to_str8(core::Allocator alloc, Vec2 v, VectorFormat format) {
  core::string_builder sb{};
  // clang-format off
    sb.pushf(alloc, VEC2_FMT_FLAGS[(usize)format.flags & 0b111],
             format.width, format.precision , v.x, 
             format.width, format.precision , v.y
             );
  // clang-format on
  return sb.commit(alloc);
}

EXPORT core::str8 to_str8(core::Allocator alloc, Vec4 v, VectorFormat format) {
  core::string_builder sb{};
  // clang-format off
    sb.pushf(alloc, VEC4_FMT_FLAGS[(usize)format.flags & 0b111],
             format.width, format.precision , v.x, 
             format.width, format.precision , v.y, 
             format.width, format.precision , v.z, 
             format.width, format.precision , v.w
             );
  // clang-format on
  return sb.commit(alloc);
}

} // namespace math
