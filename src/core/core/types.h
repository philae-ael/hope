// IWYU pragma: private

#ifndef INCLUDE_CORE_TYPE_H_
#define INCLUDE_CORE_TYPE_H_

#include <bit>
#include <concepts>
#include <stddef.h>
#include <type_traits>
#include <utility>

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

inline auto identity_f = [](auto&& x) {
  return FWD(x);
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

// === Type Lists ===

template <class T, size_t idx>
struct TYindex {
  using type                         = T;
  static constexpr std::size_t index = idx;
};

template <class... Ts>
struct ListOfTypes {
  template <template <class... Tss> typename Templa>
  using apply_to = Templa<Ts...>;

  template <class F>
  inline static auto foreach (F f) {
    foreach_impl(f, std::make_integer_sequence<std::size_t, sizeof...(Ts)>{});
  }

  template <class T, class F>
  inline static T map_construct(F f) {
    return map_construct_impl<T, F>(f, std::make_integer_sequence<std::size_t, sizeof...(Ts)>{});
  }

private:
  template <class F, std::size_t... Is>
  inline static auto foreach_impl(F f, std::integer_sequence<std::size_t, Is...>) {
    (f(TYindex<Ts, Is>{}), ...);
  }

  template <class T, class F, std::size_t... Is>
  inline static T map_construct_impl(F f, std::integer_sequence<std::size_t, Is...>) {
    return T{f(TYindex<Ts, Is>{})...};
  }
};

namespace detail_ {
template <class T>
struct storage_traits {
  using type      = T;
  using pointer   = T*;
  using reference = T&;
};

template <class T>
struct storage_traits<T&> {
  using type      = ref_wrapper<T>;
  using pointer   = T*;
  using reference = T&;
};
} // namespace detail_

template <class T>
struct MaybeUninit {
  alignas(T) u8 data[sizeof(T)];

  MaybeUninit() {}
  MaybeUninit(auto&&... args) {
    new (data) T{FWD(args)...};
  }

  MaybeUninit(const MaybeUninit&)            = default;
  MaybeUninit(MaybeUninit&&)                 = default;
  MaybeUninit& operator=(const MaybeUninit&) = default;
  MaybeUninit& operator=(MaybeUninit&&)      = default;
  T& operator*() {
    return *reinterpret_cast<T*>(data);
  }
  const T& operator*() const {
    return *reinterpret_cast<const T*>(data);
  }
};

// === Maybe ===

template <class T>
struct Maybe {
  using pointer   = typename detail_::storage_traits<T>::pointer;
  using reference = typename detail_::storage_traits<T>::reference;
  using storage   = typename detail_::storage_traits<T>::type;

  enum class Discriminant : u8 { None, Some } d = Discriminant::None;
  MaybeUninit<storage> data;

  inline constexpr auto operator==(const Maybe& other) const
    requires std::equality_comparable<T>
  {
    return d == other.d && (d == Discriminant::None || *data == *other.data);
  }
  inline constexpr auto operator!=(const Maybe& other) const
    requires std::equality_comparable<T>
  {
    return d != other.d || (d == Discriminant::Some && *data != *other.data);
  }

  static Maybe None() {
    return {};
  };
  static Maybe Some(auto&&... args) {
    return {inplace, FWD(args)...};
  }

  Maybe() {}
  Maybe(inplace_t, auto&&... args)
      : d(Discriminant::Some)
      , data{FWD(args)...} {}

  template <class Arg>
  Maybe(Arg&& arg)
    requires(std::is_convertible_v<Arg, T>)
      : d(Discriminant::Some)
      , data{FWD(arg)} {}

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
    return *data;
  }
  inline reference operator*() {
    return value();
  }
  inline pointer operator->() {
    return &*data;
  }

  T expect(const char* msg) {
    if (is_none()) {
      panic("Maybe is unexpectedly empty: %s", msg);
    }
    return *data;
  }
  Maybe<std::remove_cvref_t<T>> copied() const {
    if (d == Discriminant::None) {
      return {};
    }
    return *data;
  }
};

template <class T>
Maybe<T> None() {
  return Maybe<T>::None();
}
template <class T>
Maybe<T> Some(T t) {
  return Maybe<T>::Some(t);
}

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

template <class T, class F, class... Args>
T map_construct(tuple<Args...>& t, F f) {
  return ListOfTypes<Args...>::template map_construct<T>(
      [&t, &f]<class Ti, size_t idx>(TYindex<Ti, idx>) -> decltype(auto) { return f(get<idx>(t)); }
  );
}

template <class T, class F, class... Args>
T map_construct(const tuple<Args...>& t, F f) {
  return ListOfTypes<Args...>::template map_construct<T>(
      [&t, &f]<class Ti, size_t idx>(TYindex<Ti, idx>) -> decltype(auto) { return f(get<idx>(t)); }
  );
}

// === defer ===

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

namespace detail_ {
template <class T, class underlying>
struct handle_impl {
  enum class handle_t : underlying {};
};
} // namespace detail_

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
      static_cast<std::underlying_type<T>::type>(lhs) | static_cast<std::underlying_type<T>::type>(rhs)
  );
}

template <class T>
  requires std::is_enum_v<T>
constexpr T enum_and(T lhs, T rhs) {
  return static_cast<T>(
      static_cast<std::underlying_type<T>::type>(lhs) & static_cast<std::underlying_type<T>::type>(rhs)
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
  Idx start;
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
    return {.cur = start, .end_ = end};
  }

  struct range_iter_rev : cpp_iter<Idx, range_iter_rev> {
    using Item = Idx;
    Idx start;
    Idx cur;

    Maybe<Item> next() {
      if (start < cur) {
        return --cur;
      }

      return {};
    }
  };
  range_iter_rev iter_rev() {
    return {.start = start, .cur = end};
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
  const S& operator[](usize index) const {
    return data[index];
  }

  template <class U, class RangeIter>
  struct storage_iter : cpp_iter<U&, storage_iter<U, RangeIter>> {
    using Item = U&;
    U* v;
    RangeIter it;

    storage_iter(U* v, RangeIter it)
        : v(v)
        , it(it) {}

    Maybe<Item> next() {
      auto idx = it.next();
      if (idx.is_none()) {
        return {};
      }
      return v[idx.value()];
    }
  };
  auto indices() const {
    return range{0zu, size};
  }
  auto iter() {
    return storage_iter{data, range{0zu, size}.iter()};
  }
  auto iter() const {
    return storage_iter{data, range{0zu, size}.iter()};
  }

  auto iter_rev() {
    return storage_iter{data, range{0zu, size}.iter_rev()};
  }
  auto iter_rev() const {
    return storage_iter{data, range{0zu, size}.iter_rev()};
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
  operator core::storage<S>() {
    return {N, data};
  }
  operator core::storage<const S>() const {
    return {N, data};
  }
  core::storage<S> storage() {
    return {N, data};
  }
  core::storage<const S> storage() const {
    return {N, data};
  }
  inline S& operator[](auto idx) {
    return data[idx];
  }

  inline const S& operator[](auto idx) const {
    return data[idx];
  }
  auto iter() {
    return core::storage{*this}.iter();
  }
  auto iter() const {
    return core::storage{*this}.iter();
  }
  usize size() const {
    return N;
  }
};
template <class Arg, class... Args>
array(Arg&& arg, Args&&... args) -> array<Arg, 1 + sizeof...(Args)>;

template <class Idx, iterable It>
struct EnumerateItem {
  usize _1;
  typename detail_::storage_traits<typename It::Item>::type _2;
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
      return {inplace, count++, n.value()};
    }
    return {};
  }
};

template <iterable... Its>
struct zipiter : cpp_iter<tuple<typename detail_::storage_traits<typename Its::Item>::type...>, zipiter<Its...>> {
  using Item = tuple<typename detail_::storage_traits<typename Its::Item>::type...>;
  core::tuple<Its...> its;
  bool fused = false;

  zipiter(Its... its)
      : its{its...} {}

  Maybe<Item> next() {
    if (fused) {
      return {};
    }

    bool is_some = true;

    // Hum... how beautiful
    auto tuple_of_maybes = map_construct<tuple<Maybe<typename Its::Item>...>>(its, [&is_some](auto& it) {
      decltype(auto) v = it.next();
      is_some          = is_some && v.is_some();
      return v;
    });

    if (!is_some) {
      fused = true;
      return {};
    }

    return map_construct<Item>(tuple_of_maybes, [](auto& v) -> decltype(auto) { return v.value(); });
  }
};
} // namespace core

#endif // INCLUDE_CORE_TYPE_H_
