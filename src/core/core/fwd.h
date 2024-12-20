#ifndef INCLUDE_CORE_FWD_H_
#define INCLUDE_CORE_FWD_H_

// IWYU pragma: begin_exports
#include "base.h"
#include "platform.h"
// IWYU pragma: end_exports

namespace core {
struct Arena;
struct Allocator;
struct Scratch;

struct str8;
struct hstr8;
struct string8;
struct string_builder;

template <class T>
Maybe<T> from_hstr8(hstr8);

} // namespace core

#endif // INCLUDE_CORE_FWD_H_
