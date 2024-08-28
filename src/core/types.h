// IWYU pragma: private

#ifndef INCLUDE_CORE_TYPE_H_
#define INCLUDE_CORE_TYPE_H_

#include <bit>
#include <stddef.h>
#include <type_traits>

#include "base.h"
#include "debug.h"
namespace core {

struct inplace_t {};
constexpr inplace_t inplace{};

struct unsafe_t {};
constexpr unsafe_t unsafe{};

struct noalloc_t {};
constexpr noalloc_t noalloc{};

template <class T>
struct identity {
  using type = T;
};

template <class T>
using identity_t = identity<T>::type;
template <class T>
constexpr identity_t<T> identity_v;

template <class T>
struct ref_wrapper {
  T* ref;
  ref_wrapper(T& t)
      : ref(&t) {}

  T& operator*() {
    return *ref;
  }
  const T& operator*() const {
    return *ref;
  }
  T* operator->() {
    return ref;
  }
  const T* operator->() const {
    return ref;
  }
  operator T*() {
    return ref;
  }
  operator T&() {
    return *ref;
  }
  operator T*() const {
    return ref;
  }
  operator T&() const {
    return *ref;
  }
};

namespace detail_ {
template <class T>
struct maybe_traits {
  using type      = T;
  using pointer   = T*;
  using reference = T&;
};

template <class T>
struct maybe_traits<T&> {
  using type      = ref_wrapper<T>;
  using pointer   = T*;
  using reference = T&;
};
} // namespace detail_

template <class T>
struct Maybe {
  using pointer   = typename detail_::maybe_traits<T>::pointer;
  using reference = typename detail_::maybe_traits<T>::reference;

  enum class Discriminant : u8 { None, Some } d;
  union {
    typename detail_::maybe_traits<T>::type t;
  };

  Maybe(T t)
      : d(Discriminant::Some)
      , t(t) {}
  template <class... Args>
  Maybe(inplace_t, Args&&... args)
      : d(Discriminant::Some)
      , t(FWD(args)...) {}
  Maybe()
      : d(Discriminant::None) {}
  template <class C>
  Maybe(Maybe<C>& c)
      : Maybe(T(c)) {}
  Maybe(const Maybe&)            = default;
  Maybe(Maybe&&)                 = default;
  Maybe& operator=(const Maybe&) = default;
  Maybe& operator=(Maybe&&)      = default;

  inline bool is_some() {
    return d == Discriminant::Some;
  }
  inline bool is_none() {
    return d == Discriminant::None;
  }
  inline reference value() {
    return t;
  }
  inline reference operator*() {
    return value();
  }
  inline pointer operator->() {
    return &t;
  }
};

template <class F>
struct defer_t : F {
  ~defer_t() {
    (*this)();
  }
};
struct defer_builder_t {
  constexpr auto operator+(auto f) const {
    return defer_t{FWD(f)};
  }
};
constexpr defer_builder_t defer_builder{};

#define defer auto CONCAT(defer__, __COUNTER__) = ::core::defer_builder + [&]

template <class T, size_t alignement = alignof(T)>
struct TaggedPtr {
  static constexpr usize bit_available = std::countr_zero(alignement);
  static_assert(alignement == 1 << bit_available);
  static constexpr usize mask_tag = (1 << bit_available) - 1;
  static constexpr usize mask_ptr = ~mask_tag;

  T* ptr() {
    return (T*)(ptr_ & mask_ptr);
  }
  T* ptr_expecting(uptr tag) {
    if (this->tag() == tag) {
      return ptr();
    } else {
      return nullptr;
    }
  }
  uptr tag() {
    return ptr_ & mask_tag;
  }
  TaggedPtr with_tag(uptr tag) {
    return {ptr(), tag};
  }

  uptr ptr_ = (uptr) nullptr;

  TaggedPtr() {}
  TaggedPtr(T* p, uptr tag)
      : ptr_((uptr)p | (tag & mask_tag)) {
    DEBUG_ASSERTM(((uptr)p & mask_ptr) == (uptr)p, "pointer is missaligned and can't be tagged!");
  }
  TaggedPtr(T* p)
      : TaggedPtr(p, 0) {}
};

/// \cond internal
namespace detail_ {
template <class T, class underlying>
struct handle_impl {
  enum class handle_t : underlying {};
};
} // namespace detail_
/// \endcond

template <class T, class underlying = usize>
using handle_t = detail_::handle_impl<T, underlying>::handle_t;

template <class T, class underlying>
underlying to_underlying(handle_t<T, underlying> h) {
  return static_cast<underlying>(h);
}

namespace enum_helpers {
template <class T>
  requires std::is_enum_v<T>
constexpr T enum_or(T lhs, T rhs) {
  return static_cast<T>(
      static_cast<std::underlying_type<T>::type>(lhs) |
      static_cast<std::underlying_type<T>::type>(rhs)
  );
}

template <class T>
  requires std::is_enum_v<T>
constexpr T enum_and(T lhs, T rhs) {
  return static_cast<T>(
      static_cast<std::underlying_type<T>::type>(lhs) &
      static_cast<std::underlying_type<T>::type>(rhs)
  );
}

template <class T>
  requires std::is_enum_v<T>
constexpr T operator|(T lhs, T rhs) {
  return enum_or(lhs, rhs);
}

template <class T>
  requires std::is_enum_v<T>
constexpr T operator&(T lhs, T rhs) {
  return enum_and(lhs, rhs);
}

template <class T>
  requires std::is_enum_v<T>
constexpr bool any(T t) {
  return static_cast<std::underlying_type_t<T>>(t) != 0;
}

} // namespace enum_helpers

template <class S = u8>
struct storage {
  usize size{};
  S* data{};

  storage() = default;

  template <usize len>
  storage(S (&a)[len])
      : size(len)
      , data(a) {}

  storage(usize size, S* data)
      : size(size)
      , data(data) {}

  template <class T>
  static storage from(unsafe_t, T& t) {
    static_assert(sizeof(T) % sizeof(S) == 0);
    return {sizeof(T) / sizeof(S), (S*)&t};
  }

  storage<u8> into_bytes() {
    return {sizeof(S) * size, (u8*)data};
  }

  S& operator[](usize index) {
    return data[index];
  }
};

} // namespace core

#endif // INCLUDE_CORE_TYPE_H_
