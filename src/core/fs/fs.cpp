#define _CRT_SECURE_NO_WARNINGS

#include "fs.h"
#include "core/containers/vec.h"
#include "core/core/fwd.h"
#include <cerrno>
#include <core/containers/pool.h>
#include <core/core.h>

#include <cstdio>

#include <filesystem>

namespace std {
namespace fs = std::filesystem;
}

namespace {
struct fstree {
  core::vec<fstree> childs;
  core::hstr8 path;
  core::Maybe<core::hstr8> target;

  fstree& insert_child(core::Arena& arena, core::hstr8 path) {
    childs.push(arena, {{}, path, {}});
    return childs[childs.size() - 1];
  }
  core::Maybe<fstree&> find_child(core::hstr8 s) {
    for (auto& c : childs.iter()) {
      if (c.path == s) {
        return c;
      }
    }

    return {};
  }
};
} // namespace

namespace fs {

struct watch {
  usize id;
  const char* path;
  std::fs::file_time_type last_modified_time;
  void* userdata;
  on_file_modified_t callback;
};

struct {
  core::Arena* arena;
  fstree root;
  core::vec<watch> watchs;
  usize watch_id;
} fs;

using namespace core::literals;

EXPORT void init() {
  fs.arena = &core::arena_alloc();
}

EXPORT void mount(core::str8 path, core::str8 target) {
  auto parts = core::split{path, '/'};
  LOG2_TRACE("registering path ", path, " to ", target);

  auto* t = &fs.root;
  for (auto part : parts) {
    if (part.len == 0) {
      continue;
    }
    auto hpart = part.hash();

    auto pth = t->find_child(hpart);
    if (pth.is_none()) {
      t = &t->insert_child(*fs.arena, intern(part.hash()));
    } else {
      t = &pth.value();
    }
  }
  t->target = core::intern(target.hash());
}

EXPORT core::Maybe<core::str8> resolve_path(core::Allocator alloc, core::str8 path) {
  auto tmp = fs.arena->make_temp();
  defer { tmp.retire(); };

  auto parts = core::split{path, '/'};

  auto* t                         = &fs.root;
  core::Maybe<core::hstr8> target = fs.root.target;
  core::str8 rest                 = path;
  while (true) {
    auto part_ = parts.next();
    if (part_.is_none()) {
      break;
    }
    auto part = part_.value();
    if (part.len == 0) {
      continue;
    }

    auto pth = t->find_child(part.hash());
    if (pth.is_none()) {
      break;
    }

    t = &pth.value();
    if (t->target.is_some()) {
      target = t->target.value();
      rest   = parts.rest;
    }
  }

  if (target.is_none()) {
    return {};
  }
  core::string_builder sb{};
  sb.push(*tmp, *target);
  sb.push(*tmp, rest);

  return sb.commit(alloc, core::str8::from("/"));
}

EXPORT core::storage<u8> read_all(core::Allocator alloc, core::str8 path) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  LOG2_TRACE("reading file ", path);
  auto realpath = resolve_path(*scratch, path).expect("can't find path").cstring(*scratch);

  FILE* f = fopen(realpath, "rb");
  if (f == nullptr) {
    LOG_BUILDER(
        core::LogLevel::Error, with_stacktrace().panic().pushf("can't open file %s: %s", realpath, strerror(errno))
    );
  }

  fseek(f, 0L, SEEK_END);
  usize sz = (usize)ftell(f);
  fseek(f, 0, SEEK_SET);

  auto storage    = alloc.allocate_array<u8>(sz);
  usize data_read = fread(storage.data, 1, sz, f);
  ASSERT(data_read == sz);

  fclose(f);
  return storage;
}

EXPORT on_file_modified_handle
register_modified_file_callback(core::str8 path, on_file_modified_t callback, void* userdata) {
  const char* cpath = resolve_path(*fs.arena, path).expect("can't resolve path").cstring(*fs.arena);

  LOG_TRACE("registering path %s to be watched", cpath);

  std::error_code ec;
  auto ftime = std::filesystem::last_write_time(cpath, ec);
  if (ec) {
    ftime = std::fs::file_time_type::min();
  }

  fs.watchs.push(
      *fs.arena,
      watch{
          fs.watch_id++,
          cpath,
          ftime,
          userdata,
          callback,
      }
  );

  return on_file_modified_handle{fs.watchs[fs.watchs.size() - 1].id};
}

EXPORT void process_modified_file_callbacks() {
  for (auto& w : fs.watchs.iter()) {
    bool dirty = false;
    std::error_code ec;
    auto ftime = std::fs::last_write_time(w.path, ec);
    if (ec) {
      LOG_WARNING("error on file %s ", w.path);
      ftime = std::fs::file_time_type::min();
    } else if (w.last_modified_time < ftime) {
      dirty = true;
    }

    w.last_modified_time = ftime;

    if (dirty) {
      LOG_TRACE("path %s changed!", w.path);
      w.callback(w.userdata);
    }
  }
}

// A lot of memory can be leaked un register/unregister are called too often...
EXPORT void unregister_modified_file_callback(on_file_modified_handle h) {
  for (auto& w : fs.watchs.iter()) {
    if (w.id == (usize)h) {
      LOG_TRACE("unregistering path %s to be watched", w.path);
      SWAP(w, fs.watchs[fs.watchs.size() - 1]);
      fs.watchs.pop(core::noalloc);
      return;
    }
  }
  LOG_WARNING("on file modified handle not found!");
}

} // namespace fs
