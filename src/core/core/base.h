// IWYU pragma: private
#ifndef INCLUDE_CORE_BASE_H_
#define INCLUDE_CORE_BASE_H_

#include "platform.h" // IWYU pragma: export

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits.h>
#include <new>
#include <type_traits>
#include <utility>

#define FWD(t) static_cast<decltype(t)&&>(t)

template <typename T, size_t N>
char (&_ArraySizeHelper(T (&arr)[N]))[N];
#define ARRAY_SIZE(arr) (sizeof(_ArraySizeHelper(arr)))

#define INDEX_OF(item, arr) (ptrdiff_t(item - arr) / ptrdiff_t(sizeof(arr[0])))

#if GCC || CLANG
  #define PRINTF_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
  #define EXPORT __attribute__((visibility("default")))
#elif MSVC
  #define PRINTF_ATTRIBUTE(a, b)
  #define EXPORT
#else
  #error platform not supported
#endif

#if WINDOWS
  #define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
  #define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SWAP(a, b)  \
  do {              \
    auto tmp = b;   \
    b        = a;   \
    a        = tmp; \
  } while (0)

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)
#define GB(x) ((x) * 1024 * 1024 * 1024)

#define STRINGIFY_(c) #c
#define STRINGIFY(c) STRINGIFY_(c)
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define EVAL(a) a

#define ALIGN_MASK_DOWN(x, mask) ((x) & ~(mask))
#define ALIGN_DOWN(x, AMOUNT) ((decltype(x))ALIGN_MASK_DOWN((uptr)(x), AMOUNT - 1))

#define ALIGN_MASK_UP(x, mask) (((x) + (mask)) & (~(mask)))
#define ALIGN_UP(x, AMOUNT) ((decltype(x))ALIGN_MASK_UP((uptr)(x), AMOUNT - 1))

#define TAG(name)                     \
  constexpr struct CONCAT(name, _t) { \
  } name {}

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using usize = std::size_t;
using uptr  = std::uintptr_t;

using f32 = float;
static_assert(sizeof(f32) == 4);
using f64 = double;
static_assert(sizeof(f64) == 8);

constexpr usize max_align = alignof(std::max_align_t);

#if GCC || CLANG
  #define TRAP asm volatile("int $3")
#elif MSVC
  #define TRAP __debugbreak()
#else
  #error NOT IMPLEMENTED
#endif

#define ASSERT(cond)                                           \
  do {                                                         \
    if (!(cond)) [[unlikely]] {                                \
      ::core::panic("assertion `%s` failed", STRINGIFY(cond)); \
    }                                                          \
  } while (0)

#define ASSERTEQ(a, b)                                                                                          \
  do {                                                                                                          \
    auto __a          = (a);                                                                                    \
    decltype(__a) __b = (b);                                                                                    \
    if (!(__a == __b)) [[unlikely]] {                                                                           \
      ::core::panic("assertion `%s == %s` failed: `%g != %g`", STRINGIFY(a), STRINGIFY(b), (f32)__a, (f32)__b); \
    }                                                                                                           \
  } while (0)

#define ASSERTM(cond, fmt, ...)                                                                 \
  do {                                                                                          \
    if (!(cond)) [[unlikely]] {                                                                 \
      ::core::panic("assertion `%s` failed:\n" fmt, STRINGIFY(cond) __VA_OPT__(, __VA_ARGS__)); \
    }                                                                                           \
  } while (0)

#ifdef DEBUG
  #define DEBUG_STMT(f) f
#else
  #define DEBUG_STMT(f)
#endif

#define DEBUG_ASSERT(...) DEBUG_STMT(ASSERT(__VA_ARGS__))
#define DEBUG_ASSERTM(...) DEBUG_STMT(ASSERTM(__VA_ARGS__))

#ifdef DEBUG
  #define ASSERT_INVARIANT(x) ASSERT(x)
#else
  #define ASSERT_INVARIANT(x) [[assume(x)]]
#endif

#define DROP_4_ARG(a1, a2, a3, a4, ...) (__VA_ARGS__)
#define todo(...)                                                                                                  \
  core::panic __VA_OPT__(DROP_4_ARG)(                                                                              \
      "function %s has not been implemented in file %s:%d", __func__, __FILE__, __LINE__ __VA_OPT__(, __VA_ARGS__) \
  )

namespace core {

struct Allocator;

TAG(unit);
TAG(inplace);
TAG(unsafe);
TAG(clear);
TAG(noalloc);
TAG(iter_end);
TAG(cstr);
TAG(bitcast);

[[noreturn]] void panic(const char* msg, ...) PRINTF_ATTRIBUTE(1, 2);

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
    DEBUG_ASSERT(is_some());
    return *data;
  }
  inline reference operator*() {
    return value();
  }
  inline pointer operator->() {
    return &value();
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

  template <class T, usize len>
  storage(T (&a)[len])
      : size(len)
      , data(a) {}

  template <class T, usize len>
  storage(T (&&a)[len])
      : size(len)
      , data(a) {}

  storage(usize size, S* data)
      : size(size)
      , data(data) {}

  storage(std::initializer_list<S> l)
      // requires std::is_const_v<S> // it's unsound to
      : size(l.size())
      , data(const_cast<S*>(std::begin(l))) {}

  storage(S& s)
      : size(1)
      , data(&s) {}

  template <class T>
  storage(const storage<T>& s)
      : size(s.size)
      , data(s.data) {}

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
  auto begin() = delete /*("use the iter() method")*/;
  auto end()   = delete /*("use the iter() method")*/;
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

  storage slice(usize len, usize offset = 0) {
    ASSERT(len + offset <= size);
    return {len, data + offset};
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
    return core::storage<S>{*this}.iter();
  }
  auto iter() const {
    return core::storage<const S>{*this}.iter();
  }
  constexpr usize size() const {
    return N;
  }

  constexpr auto operator<=>(const array& other) const = default;
};
template <class Arg, class... Args>
array(Arg arg, Args... args) -> array<Arg, 1 + sizeof...(Args)>;

struct hstr8 {
  u64 hash;
  usize len;
  const u8* data;

  // This is not a real == operator bc collisions but well, it will be a fun bug to find if a
  // collision occur
  constexpr bool operator==(const hstr8& other) const {
    return other.hash == hash;
  }
  hstr8 clone(Allocator alloc);
  const char* cstring(Allocator alloc);
};

struct hasher {
  u64 h = 0xcbf29ce484222325;

  constexpr void hash(const u8* data, size_t len) {
    for (usize i = 0; i < len; i++) {
      h *= 0x100000001b3;
      h ^= u64(data[i]);
    }
  }

  constexpr void hash(const storage<u8> data) {
    hash(data.data, data.size);
  }

  template <class D>
    requires std::is_trivial_v<D>
  constexpr void hash(const D& d) {
    auto s = std::bit_cast<core::array<u8, sizeof(D)>>(d);
    hash(s.storage());
  }

  constexpr u64 value() {
    return h;
  }
};

struct str8 {
  usize len;
  const u8* data;

  template <size_t len>
  static str8 from(const char (&d)[len]) {
    return from(d, len - 1);
  }
  static str8 from(const char* d, size_t len) {
    return str8{len, reinterpret_cast<const u8*>(d)};
  }
  static constexpr str8 from(const u8* d, size_t len) {
    return str8{len, d};
  }
  static str8 from(cstr_t, const char* d) {
    return str8{strlen(d), reinterpret_cast<const u8*>(d)};
  }
  constexpr hstr8 hash() const {
    hasher h{};
    h.hash(data, len);
    return {
        h.value(),
        len,
        data,
    };
  }

  inline str8 subslice(usize offset) const {
    return {len - offset, data + offset};
  }
  inline str8 subslice(usize offset, const usize len) const {
    return {len, data + offset};
  }

  inline constexpr u8 operator[](usize idx) const {
    return data[idx];
  }

  str8 clone(Allocator alloc);
  const char* cstring(Allocator alloc);

  constexpr auto operator<=>(const str8& other) const {
    return std::lexicographical_compare_three_way(data, data + len, other.data, other.data + len);
  }
  constexpr bool operator==(const str8& other) const {
    return (*this <=> other) == std::strong_ordering::equal;
  };
};

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

// === Tuple Definition ===
// Hand made tuples is way easier and should compile way faster also, but's it a bit of code, either generate them or

template <class... T>
struct tuple;

template <usize N, class... Args>
  requires(N < sizeof...(Args))
decltype(auto) get(tuple<Args...>& t) {
  return t.template get<N>();
}

template <usize N, class... Args>
  requires(N < sizeof...(Args))
decltype(auto) get(const tuple<Args...>& t) {
  return t.template get<N>();
}

template <usize N, class... Args>
  requires(N < sizeof...(Args))
decltype(auto) get(tuple<Args...>&& t) {
  return t.template get<N>();
}

template <>
struct tuple<> {};

#define _TUPLE_REPEAT_0(D, J)
#define _TUPLE_REPEAT_1(D, J) D(0)
#define _TUPLE_REPEAT_2(D, J) _TUPLE_REPEAT_1(D, J) J() D(1)
#define _TUPLE_REPEAT_3(D, J) _TUPLE_REPEAT_2(D, J) J() D(2)
#define _TUPLE_REPEAT_4(D, J) _TUPLE_REPEAT_3(D, J) J() D(3)
#define _TUPLE_REPEAT_5(D, J) _TUPLE_REPEAT_4(D, J) J() D(4)
#define _TUPLE_REPEAT_6(D, J) _TUPLE_REPEAT_5(D, J) J() D(5)
#define _TUPLE_REPEAT_7(D, J) _TUPLE_REPEAT_6(D, J) J() D(6)
#define _TUPLE_REPEAT_8(D, J) _TUPLE_REPEAT_7(D, J) J() D(7)
#define _TUPLE_REPEAT_9(D, J) _TUPLE_REPEAT_8(D, J) J() D(8)

#define _TUPLE_COMMA_ID() ,
#define _TUPLE_SEMICOLON_ID() ;

#define _TUPLE_REPEAT(N, ...) _TUPLE_REPEAT_##N(__VA_ARGS__)

#define _TUPLE_DEFINE_TEMPLATE_DEF(k) class T##k
#define _TUPLE_DEFINE_TEMPLATE(k) T##k
#define _TUPLE_DEFINE_FIELD(k) NO_UNIQUE_ADDRESS T##k _##k
#define _TUPLE_DEFINE_GET_INDEX(k) \
  if constexpr (N == k)            \
  return self._##k

#define _TUPLE_DEFINE(k)                                                    \
  template <_TUPLE_REPEAT(k, _TUPLE_DEFINE_TEMPLATE_DEF, _TUPLE_COMMA_ID)>  \
  struct tuple<_TUPLE_REPEAT(k, _TUPLE_DEFINE_TEMPLATE, _TUPLE_COMMA_ID)> { \
    _TUPLE_REPEAT(k, _TUPLE_DEFINE_FIELD, _TUPLE_SEMICOLON_ID);             \
    template <usize N>                                                      \
      requires(N < k)                                                       \
    auto& get(this auto& self) {                                            \
      _TUPLE_REPEAT(k, _TUPLE_DEFINE_GET_INDEX, _TUPLE_SEMICOLON_ID);       \
    }                                                                       \
    auto operator<=>(const tuple& other) const noexcept = default;          \
  };

_TUPLE_DEFINE(1)
_TUPLE_DEFINE(2)
_TUPLE_DEFINE(3)
_TUPLE_DEFINE(4)
_TUPLE_DEFINE(5)
_TUPLE_DEFINE(6)
_TUPLE_DEFINE(7)
_TUPLE_DEFINE(8)
_TUPLE_DEFINE(9)

#undef _TUPLE_DEFINE_TEMPLATE_DEF
#undef _TUPLE_DEFINE_TEMPLATE
#undef _TUPLE_DEFINE_FIELD
#undef _TUPLE_DEFINE_GET_INDEX
#undef _TUPLE_COMMA_ID
#undef _TUPLE_SEMICOLON_ID
#undef _TUPLE_DEFINE
#undef _TUPLE_REPEAT
#undef _TUPLE_REPEAT_0
#undef _TUPLE_REPEAT_1
#undef _TUPLE_REPEAT_2
#undef _TUPLE_REPEAT_3
#undef _TUPLE_REPEAT_4
#undef _TUPLE_REPEAT_5
#undef _TUPLE_REPEAT_6
#undef _TUPLE_REPEAT_7
#undef _TUPLE_REPEAT_8
#undef _TUPLE_REPEAT_9

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
//
template <class Idx, iterable It>
struct EnumerateItem : tuple<usize, typename detail_::storage_traits<typename It::Item>::type> {};

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

template <iterable It, class Idx = usize>
struct enumerate_rev : cpp_iter<EnumerateItem<Idx, It>, enumerate_rev<It, Idx>> {
  It it;
  Idx count;

  enumerate_rev(It it, Idx count)
      : it(it)
      , count(count) {}

  using Item = EnumerateItem<Idx, It>;

  Maybe<Item> next() {
    auto n = it.next();
    if (n.is_some()) {
      return {inplace, count--, n.value()};
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
    auto tuple_of_maybes = map_construct<tuple<Maybe<typename Its::Item>...>>(its, [&is_some](auto&& it) {
      decltype(auto) v = FWD(it).next();
      is_some          = is_some && v.is_some();
      return v;
    });

    if (!is_some) {
      fused = true;
      return {};
    }

    return map_construct<Item>(tuple_of_maybes, [](auto&& v) -> decltype(auto) { return FWD(v).value(); });
  }
};

struct source_location {
  str8 file;
  str8 func;
  u32 line;
};

#define CURRENT_SOURCE_LOCATION                                          \
  ::core::source_location {                                              \
    ::core::str8::from(__FILE__), ::core::str8::from(__func__), __LINE__ \
  }

void dump_backtrace(usize skip = 1);

void setup_crash_handler();

inline void blackbox(auto& x) {
#if LINUX
  void* ptr = &x;
  asm volatile("nop" : "+rm"(ptr));
#else
    // blackbox will do nothing
#endif
}

} // namespace core

#endif // INCLUDE_CORE_BASE_H_
