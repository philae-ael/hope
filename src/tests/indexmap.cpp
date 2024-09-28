#include "core/core.h"
#include "tests.h"
#include <core/containers/indexmap.h>

TEST(indexmap compiles) {
  core::index_map<int> im{};
  (void)im;
}

TEST(indexmap insert) {
  core::index_map<int> im{};
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;

  auto h1 = im.new_handle(alloc);
  auto h2 = im.new_handle(alloc);
  tassert(im.resolve(h1) == core::Maybe<u32>{0}, "invalid 1st handle");
  tassert(im.resolve(h2) == core::Maybe<u32>{1}, "invalid 2nd handle");

  auto command = im.free_handle(h1);
  tassert(command.tag == decltype(command)::Tag::SwapDeleteLast, "invalid command after free");
  tassert(command.swap_delete_last.idx == 0, "invalid command after free");
  tassert(im.resolve(h1) == core::Maybe<u32>{}, "invalid 1st handle after free");
  tassert(im.resolve(h2) == core::Maybe<u32>{0}, "invalid 2nd handle after free");

  auto h3 = im.new_handle(alloc);
  tassert(im.resolve(h3) == core::Maybe<u32>{1}, "invalid 3nd handle");

  command = im.free_handle(h3);
  tassert(command.tag == decltype(command)::Tag::DeleteLast, "invalid command after free");
}

TEST(indexmap free with known last) {
  core::index_map<int> im{};
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;

  auto h1 = im.new_handle(alloc);
  auto h2 = im.new_handle(alloc);
  tassert(im.resolve(h1) == core::Maybe<u32>{0}, "invalid 1st handle");
  tassert(im.resolve(h2) == core::Maybe<u32>{1}, "invalid 2nd handle");

  auto command = im.free_handle(h1, h2);
  tassert(command.tag == decltype(command)::Tag::SwapDeleteLast, "invalid command after free");
  tassert(command.swap_delete_last.idx == 0, "invalid command after free");
  tassert(im.resolve(h1) == core::Maybe<u32>{}, "invalid 1st handle after free");
  tassert(im.resolve(h2) == core::Maybe<u32>{0}, "invalid 2nd handle after free");

  auto h3 = im.new_handle(alloc);
  tassert(im.resolve(h3) == core::Maybe<u32>{1}, "invalid 3nd handle");

  command = im.free_handle(h3);
  tassert(command.tag == decltype(command)::Tag::DeleteLast, "invalid command after free");
}

TEST(linear_growable_pool_with_generational_handles) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;

  core::linear_growable_pool_with_generational_handles<int> a;
  auto h1 = a.insert(alloc, 2);
  auto h2 = a.insert(alloc, 3);

  {
    auto v = a.get(h1);
    tassert(v.is_some(), "1st handle not found ");
    tassert(*v == 2, "invalid value");
    *v = 4;
  }
  {
    auto v = a.get(h2);
    tassert(v.is_some(), "2st handle not found ");
    tassert(*v == 3, "invalid value");
  }
  {
    auto v = a.get(h1);
    tassert(v.is_some(), "1st handle not found ");
    tassert(*v == 4, "invalid value");
  }
  a.destroy(h1);
  {
    auto v = a.get(h2);
    tassert(v.is_some(), "2st handle not found ");
    tassert(*v == 3, "invalid value");
  }
  a.destroy(h2);
}
