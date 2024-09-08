// IWYU pragma: private

#ifndef INCLUDE_CORE_TYPE_H_
#define INCLUDE_CORE_TYPE_H_

#include <bit>
#include <stddef.h>
#include <type_traits>

#include "base.h"
#include "debug.h"

namespace core {
TAG(unit);
TAG(inplace);
TAG(unsafe);
TAG(clear);
TAG(noalloc);

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
      : d(c.d)
      , t(c.t) {}
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

  T expect(const char* msg) {
    if (is_none()) {
      panic("Maybe is unexpectidly empty: %s", msg);
    }
    return t;
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
constexpr T& operator|=(T& lhs, T rhs) {
  return lhs = lhs | rhs;
}

template <class T>
  requires std::is_enum_v<T>
constexpr T operator&(T lhs, T rhs) {
  return enum_and(lhs, rhs);
}

template <class T>
  requires std::is_enum_v<T>
constexpr T& operator&=(T& lhs, T rhs) {
  return lhs = lhs & rhs;
}

template <class T>
  requires std::is_enum_v<T>
constexpr bool any(T t) {
  return static_cast<std::underlying_type_t<T>>(t) != 0;
}

} // namespace enum_helpers

struct iter_end_t {};
constexpr iter_end_t iter_end{};

template <class T>
concept iterable = requires(T t) {
  t.next();
  typename T::Item;
};

template <class T, class PullIter, class S = T>
struct cpp_iter {
  T operator*() {
    return iter_cur.value();
  }
  auto& operator++() {
    iter_cur = _self().next();
    return _self();
  }

  bool operator!=(iter_end_t) {
    return iter_cur.is_some();
  }

  auto& begin() {
    if (iter_cur.is_none()) {
      ++*this;
    }
    return _self();
  }
  auto end() {
    return iter_end;
  }

private:
  Maybe<S> iter_cur;

  PullIter& _self() {
    return *static_cast<PullIter*>(this);
  }
};

template <class Idx>
struct range {
  Idx begining;
  Idx end;

  struct range_iter : cpp_iter<Idx, range_iter> {
    using Item = Idx;
    Idx cur;
    Idx end_;

    Maybe<Item> next() {
      if (cur < end_) {
        return cur++;
      }

      return {};
    }
  };
  range_iter iter() {
    return {.cur = begining, .end_ = end};
  }
};

template <class S = u8>
struct storage {
  usize size{};
  S* data{};

  storage() = default;

  template <usize len>
  storage(S (&a)[len])
      : size(len)
      , data(a) {}

  // HUM... not sure i should allow that without copying... if it bites me i should only blame
  // myself i guess
  template <usize len>
  storage(S (&&a)[len])
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

  template <class U>
  struct storage_iter : cpp_iter<U&, storage_iter<U>> {
    using Item = U&;
    U* v;
    range<usize>::range_iter it;

    storage_iter(U* v, range<usize>::range_iter it)
        : v(v)
        , it(it) {}

    Maybe<Item> next() {
      auto idx = it.next();
      if (idx.is_none()) {
        return {};
      }
      return {v[idx.value()]};
    }
  };
  auto indices() const {
    return range{0zu, size};
  }
  storage_iter<S> iter() {
    return {data, range{0zu, size}.iter()};
  }
  storage_iter<const S> iter() const {
    return {data, range{0zu, size}.iter()};
  }
};

namespace detail_ {
template <class S, usize N>
struct array_storage {
  using type = S[N];
};
template <class S>
struct array_storage<S, 0> {
  using type = S*;
};
} // namespace detail_
template <class S, usize N>
struct array {
  detail_::array_storage<S, N>::type data;
  operator storage<S>() {
    return {N, data};
  }
  operator storage<const S>() const {
    return {N, data};
  }
  inline S& operator[](auto idx) {
    return data[idx];
  }

  inline const S& operator[](auto idx) const {
    return data[idx];
  }
  auto iter() {
    return storage{*this}.iter();
  }
  auto iter() const {
    return storage{*this}.iter();
  }
  usize size() const {
    return N;
  }
};
template <class... Args>
array(Args&&... args) -> array<std::common_type_t<Args...>, sizeof...(Args)>;

template <class Idx, iterable It>
struct EnumerateItem {
  usize _1;
  typename detail_::maybe_traits<typename It::Item>::type _2;
};
template <iterable It, class Idx = usize>
struct enumerate : cpp_iter<EnumerateItem<Idx, It>, enumerate<It, Idx>> {
  It it;
  Idx count{};

  enumerate(It it)
      : it(it) {}
  enumerate(It it, Idx count)
      : it(it)
      , count(count) {}

  using Item = EnumerateItem<Idx, It>;

  Maybe<Item> next() {
    auto n = it.next();
    if (n.is_some()) {
      return {{
          count++,
          n.value(),
      }};
    }
    return {};
  }
};

} // namespace core

#endif // INCLUDE_CORE_TYPE_H_
