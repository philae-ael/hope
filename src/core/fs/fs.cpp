#include "fs.h"
#include "core/containers/vec.h"
#include <cerrno>
#include <core/containers/pool.h>
#include <core/core.h>
#include <cstdio>

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

struct path_correspondance {
  core::str8 path, target;
};

struct {
  core::Arena* arena;
  fstree root;
} fs;

using namespace core::literals;

EXPORT void init(core::str8 path) {
  fs.arena = &core::arena_alloc(KB(1));

  register_path("/"_s, path);
}

EXPORT void register_path(core::str8 path, core::str8 target) {
  auto parts = core::split{path, '/'};
  LOG_BUILDER(
      core::LogLevel::Trace, push("registering path ").push(path).push(" to ").push(target)
  );

  auto* t = &fs.root;
  for (auto part : parts) {
    if (part.len == 0) {
      continue;
    }
    auto hpart = part.hash();

    auto pth   = t->find_child(hpart);
    if (pth.is_none()) {
      t = &t->insert_child(*fs.arena, hpart.clone(*fs.arena));
    } else {
      t = &pth.value();
    }
  }
  t->target = target.clone(*fs.arena).hash();
}

EXPORT core::str8 resolve_path(core::Arena& arena, core::str8 path) {
  auto tmp = fs.arena->make_temp();
  defer { tmp.retire(); };

  auto parts         = core::split{path, '/'};

  auto* t            = &fs.root;
  core::hstr8 target = fs.root.target.expect("root not set");
  core::str8 rest    = path;
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

  core::string_builder sb{};
  sb.push(*tmp, target);
  sb.push(*tmp, rest);

  return sb.commit(arena, core::str8::from("/"));
}

EXPORT core::storage<u8> read_all(core::Arena& arena, core::str8 path) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  auto realpath = resolve_path(*scratch, path).cstring(*scratch);

  FILE* f       = fopen(realpath, "r");
  if (f == nullptr) {
    LOG_BUILDER(
        core::LogLevel::Error,
        with_stacktrace().panic().pushf("can't open file: %s", strerror(errno))
    );
  }

  fseek(f, 0L, SEEK_END);
  usize sz = (usize)ftell(f);
  fseek(f, 0, SEEK_SET);

  auto storage = arena.allocate_array<u8>(sz);
  fread(storage.data, 1, sz, f);

  fclose(f);
  return storage;
}

} // namespace fs
