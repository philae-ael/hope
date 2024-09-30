#ifndef INCLUDE_CORE_TYPE_INFO_H_
#define INCLUDE_CORE_TYPE_INFO_H_

#include <typeinfo>

#include "base.h"
#include "fwd.h"

#if LINUX
  #include <cxxabi.h>
#endif

namespace core {
template <class T>
const char* type_name() {
  static const char* n = nullptr;
  if (n == nullptr) {
    const char* type_name = typeid(T).name();
#if LINUX
    int status;
    const char* t = abi::__cxa_demangle(type_name, nullptr, 0, &status);
    type_name     = t;
#endif
    n = type_name;
  }
  return n;
}

struct LayoutInfo {
  usize size;
  usize alignement;
  LayoutInfo array(usize size) {
    return {
        this->size * size,
        alignement,
    };
  }

  bool operator==(LayoutInfo& other) {
    return other.size == size && other.alignement == alignement;
  }
};

str8 to_str8(Allocator alloc, LayoutInfo);

template <class T>
inline consteval LayoutInfo default_layout_of() {
  return {sizeof(T), alignof(T)};
}

} // namespace core

#endif // INCLUDE_CORE_TYPE_INFO_H_
