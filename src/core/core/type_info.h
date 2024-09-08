#ifndef INCLUDE_CORE_TYPE_INFO_H_
#define INCLUDE_CORE_TYPE_INFO_H_

#include <typeinfo>

#include "base.h"

#if LINUX
  #include <cxxabi.h>
#endif

namespace core {
struct Arena;

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

str8 to_str8(Arena& ar, LayoutInfo);

template <class T>
constexpr LayoutInfo default_layout_of() {
  return {sizeof(T), alignof(T)};
}

struct type_info_t {
  LayoutInfo layout;
  u64 type_id;
};

template <class T>
constexpr type_info_t type_info();

#define TYPE_INFO(T, id)                 \
  template <>                            \
  constexpr type_info_t type_info<T>() { \
    return {default_layout_of<T>(), id}; \
  }

TYPE_INFO(u8, 0);
TYPE_INFO(u16, 1);
TYPE_INFO(u32, 2);
TYPE_INFO(u64, 3);
TYPE_INFO(type_info_t, 1);

} // namespace core

#endif // INCLUDE_CORE_TYPE_INFO_H_
