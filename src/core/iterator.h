#ifndef INCLUDE_CORE_ITERATOR_H_
#define INCLUDE_CORE_ITERATOR_H_

#include "base.h"
#include "types.h"
namespace core {

struct iter_end_t {};
constexpr iter_end_t iter_end{};

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

template <class T = usize>
struct range {
  T begining;
  T end;

  struct range_iter : cpp_iter<T, range_iter> {
    T cur;
    T end_;

    Maybe<T> next() {
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

} // namespace core

#endif // INCLUDE_CORE_ITERATOR_H_
