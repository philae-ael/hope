#ifndef INCLUDE_CORE_TYPE_INFO_H_
#define INCLUDE_CORE_TYPE_INFO_H_

#include <typeinfo>
#include <utility>

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

template <class T, size_t idx>
struct TYindex {
  using type                         = T;
  static constexpr std::size_t index = idx;
};

template <class... Ts>
struct ListOfTypes {
  template <template <class... Tss> typename Templa>
  using apply_to = Templa<Ts...>;

  template <class F, std::size_t... Is>
  inline static void map_apply_(F f, std::integer_sequence<std::size_t, Is...>) {
    (f(TYindex<Ts, Is>{}), ...);
  }

  template <class F>
  inline static void map(F f) {
    map_apply_(f, std::make_integer_sequence<std::size_t, sizeof...(Ts)>{});
  }

  template <class T, class F, std::size_t... Is>
  inline static T map_construct_apply_(F f, std::integer_sequence<std::size_t, Is...>) {
    return T{f(TYindex<Ts, Is>{})...};
  }

  template <class T, class F>
  inline static T map_construct(F f) {
    return map_construct_apply_<T, F>(f, std::make_integer_sequence<std::size_t, sizeof...(Ts)>{});
  }
};

} // namespace core

#endif // INCLUDE_CORE_TYPE_INFO_H_
