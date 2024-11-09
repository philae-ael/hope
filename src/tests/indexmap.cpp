#include "core/core.h"
#include "tests.h"
#include <core/containers/handle_map.h>

TEST(handle map adapter compiles) {
  core::handle_map_adapter im{};
  (void)im;
}

TEST(handle map adapter insert) {
  core::handle_map_adapter im{};
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;

  auto h1 = im.new_handle(alloc);
  auto h2 = im.new_handle(alloc);
  tassert(im.resolve(h1) == core::Some<u32>(0), "invalid 1st handle");
  tassert(im.resolve(h2) == core::Some<u32>(1), "invalid 2nd handle");

  auto command = im.free_handle(h1);
  tassert(command.tag == decltype(command)::Tag::SwapDeleteLast, "invalid command after free");
  tassert(command.swap_delete_last.idx == 0, "invalid command after free");
  tassert(im.resolve(h1) == core::None<u32>(), "invalid 1st handle after free");
  tassert(im.resolve(h2) == core::Some<u32>(0), "invalid 2nd handle after free");

  auto h3 = im.new_handle(alloc);
  tassert(im.resolve(h3) == core::Some<u32>(1), "invalid 3nd handle");
  auto h4 = im.new_handle(alloc);
  tassert(im.resolve(h4) == core::Some<u32>(2), "invalid 4nd handle");

  command = im.free_handle(h4);
  tassert(command.tag == decltype(command)::Tag::DeleteLast, "invalid command after free");
}

TEST(handle map adapter free with known last) {
  core::handle_map_adapter im{};
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;

  auto h1 = im.new_handle(alloc);
  auto h2 = im.new_handle(alloc);
  tassert(im.resolve(h1) == core::Some<u32>(0), "invalid 1st handle");
  tassert(im.resolve(h2) == core::Some<u32>(1), "invalid 2nd handle");

  auto command = im.free_handle(h1, h2);
  tassert(command.tag == decltype(command)::Tag::SwapDeleteLast, "invalid command after free");
  tassert(command.swap_delete_last.idx == 0, "invalid command after free");
  tassert(im.resolve(h1) == core::Maybe<u32>{}, "invalid 1st handle after free");
  tassert(im.resolve(h2) == core::Some<u32>(0), "invalid 2nd handle after free");

  auto h3 = im.new_handle(alloc);
  tassert(im.resolve(h3) == core::Some<u32>(1), "invalid 3nd handle");

  command = im.free_handle(h3);
  tassert(command.tag == decltype(command)::Tag::DeleteLast, "invalid command after free");
}

TEST(handle_map) {
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;

  core::handle_map<int> a;
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
TEST(handle_map iter) {
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;

  core::handle_map<int> a;
  auto h1 = a.insert(alloc, 2);
  auto h2 = a.insert(alloc, 3);

  bool h1access = false;
  bool h2access = false;
  for (auto t : a.iter_enumerate()) {
    auto handle = get<0>(t);
    auto item   = get<1>(t);
    if (handle == h1) {
      h1access = true;
      tassert(item == 2, "a[h1] == 2");
    }

    if (handle == h2) {
      h2access = true;
      tassert(item == 3, "a[h2] == 3");
    }
  }
  tassert(h2access, "h2 accesed");
  tassert(h1access, "h1 accesed");
}
