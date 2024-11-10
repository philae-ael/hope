
#include "fs.h"
#include <core/containers/pool.h>
#include <core/containers/vec.h>
#include <core/core.h>

#include <cstdio>

#include <cerrno>
#include <filesystem>
#include <uv.h>

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
  void* userdata;
  on_file_modified_t callback;
  uv_fs_event_t ev;
};

struct {
  core::Arena* arena;
  fstree root;
  core::pool<watch> watchs;
  usize watch_id;

  uv_loop_t* event_loop;
} fs;

EXPORT void init(uv_loop_t* event_loop) {
  fs.event_loop = event_loop;
  fs.arena      = &core::arena_alloc();
}

EXPORT void mount(core::str8 path, core::str8 target) {
  auto parts = core::split{path, '/'};
  LOG2_DEBUG("registering path ", path, " to ", target);

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
  t->target = core::Some(core::intern(target.hash()));
}

EXPORT core::Maybe<core::str8> resolve_path(core::Allocator alloc, core::str8 path) {
  auto* t     = &fs.root;
  auto parts  = core::split{path, '/'};
  auto& begin = parts.begin();
  auto end    = parts.end();
  core::str8 cur_part;
  for (; begin != end; ++begin) {
    cur_part = *begin;
    if (cur_part.len == 0) {
      continue;
    }

    auto pth = t->find_child(cur_part.hash());
    if (pth.is_none()) {
      break;
    }

    t = &pth.value();
  }

  if (t->target.is_none()) {
    LOG2_TRACE("could not found base ", cur_part);
    return core::None<core::str8>();
  }

  core::str8 s;
  if (parts.rest.len == 0) {
    s = core::join(alloc, PATH_SEPARATOR_S, *t->target, cur_part);
  } else {
    s = core::join(alloc, PATH_SEPARATOR_S, *t->target, cur_part, parts.rest);
  }
  LOG2_TRACE("path ", path, " has been resolved to ", s);
  return s;
}

EXPORT core::storage<u8> read_all(core::Allocator alloc, virtualpath path) {
  auto scratch = core::scratch_get();

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

  LOG2_INFO("registering path ", cpath, " (", path, ") to be watched");

  std::error_code ec;
  auto ftime = std::filesystem::last_write_time(cpath, ec);
  if (ec) {
    ftime = std::fs::file_time_type::min();
  }

  watch* w = &fs.watchs.allocate(*fs.arena);
  new (w) watch{
      fs.watch_id++,
      cpath,
      userdata,
      callback,
      // This requires the memory to be pinned! The memory in a pool is pinned so ok
      uv_fs_event_t{},
  };

  if (uv_handle_get_data((uv_handle_t*)&w->ev) == nullptr) {
    ASSERT(uv_fs_event_init(fs.event_loop, &w->ev) == 0);
  }
  uv_handle_set_data((uv_handle_t*)&w->ev, w);
  ASSERT(
      uv_fs_event_start(
          &w->ev,
          [](uv_fs_event_t* handle, const char* filename, int events, int status) {
            if ((events & UV_CHANGE) != 0) {
              LOG_TRACE("path %s changed!", filename);

              watch* w = (watch*)handle->data;
              w->callback(w->userdata);
            }
          },
          cpath, 0
      ) == 0
  );

  return on_file_modified_handle{(usize)w};
}

EXPORT void unregister_modified_file_callback(on_file_modified_handle h) {
  watch* w = (watch*)h;
  LOG_TRACE("unregistering path %s to be watched", w->path);

  ASSERT(uv_fs_event_stop(&w->ev) == 0);
  uv_close((uv_handle_t*)&w->ev, [](uv_handle_t* handle) {
    watch* w = (watch*)handle->data;
    fs.watchs.deallocate(*fs.arena, *w);
  });
  return;
}

} // namespace fs
