#ifndef INCLUDE_CONTAINERS_TUPLE_H_
#define INCLUDE_CONTAINERS_TUPLE_H_

#include "core/core/type_info.h"
#include <core/core/base.h>
#include <type_traits>

namespace core {

// === Tuple Definition ===

template <class... Args>
struct tuple;

template <>
struct tuple<> {};

template <class T, class... Args>
struct tuple<T, Args...> {
  [[no_unique_address]] T head;
  [[no_unique_address]] tuple<Args...> tail;
};

template <class... Args>
tuple(Args...) -> tuple<Args...>;

// === Typle helpers ===

namespace details {
template <class... Args>
struct all_types_are_unique;

template <class T>
struct all_types_are_unique<T> {
  static constexpr bool value = true;
};

template <class T, class... Args>
struct all_types_are_unique<T, Args...> {
  static constexpr bool value = !(std::is_same_v<T, Args> || ...) && all_types_are_unique<Args...>::value;
};

template <class... Args>
constexpr bool all_types_are_unique_v = all_types_are_unique<Args...>::value;
} // namespace details

// === Accessors ===
template <usize N, class... Args>
auto& get(tuple<Args...>& t) {
  static_assert(N < sizeof...(Args), "out of index");
  if constexpr (N == 0) {
    return t.head;
  } else {
    return get<N - 1>(t.tail);
  }
}

template <usize N, class... Args>
const auto& get(const tuple<Args...>& t) {
  static_assert(N < sizeof...(Args), "out of index");

  if constexpr (N == 0) {
    return t.head;
  } else {
    return get<N - 1>(t.tail);
  }
}

template <class T, class... Args>
auto& get_by_type(tuple<Args...>& t)
  requires details::all_types_are_unique<Args...>::value
{
  if constexpr (std::is_same_v<decltype(t.head), T>) {
    return t.head;
  } else {
    return get_by_type<T>(t.tail);
  }
}

template <class T, class... Args>
const auto& get_by_type(const tuple<Args...>& t)
  requires details::all_types_are_unique<Args...>::value
{
  if constexpr (std::is_same_v<decltype(t.head), T>) {
    return t.head;
  } else {
    return get_by_type<T>(t.tail);
  }
}

template <class T, class... Args>
T build_from_tuple(const tuple<Args...>& t) {
  return ListOfTypes<Args...>::template map_construct<T>([&t]<class Ti, size_t idx>(TYindex<Ti, idx>) {
    return get<idx>(t);
  });
}

} // namespace core

#endif // INCLUDE_CONTAINERS_TUPLE_H_
