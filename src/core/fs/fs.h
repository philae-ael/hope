#ifndef INCLUDE_FS_FS_H_
#define INCLUDE_FS_FS_H_

#include <core/core.h>

#if WINDOWS
  #define PATH_SEPARATOR_S "\\"_s
  #define PATH_SEPARATOR_C '\\'
#else
  #define PATH_SEPARATOR_S "/"_s
  #define PATH_SEPARATOR_C '/'
#endif

typedef struct uv_loop_s uv_loop_t;

namespace fs {
// TODO: make those a strong alias
using virtualpath = core::str8;
using realpath    = core::str8;

void init(uv_loop_t* loop);
void mount(virtualpath path, realpath target);
core::Maybe<realpath> resolve_path(core::Allocator alloc, virtualpath);
core::storage<u8> read_all(core::Allocator alloc, virtualpath);

using on_file_modified_t      = void (*)(void*);
using on_file_modified_handle = core::handle_t<on_file_modified_t>;
on_file_modified_handle register_modified_file_callback(virtualpath vpath, on_file_modified_t callback, void* userdata);
void unregister_modified_file_callback(on_file_modified_handle h);

} // namespace fs

#endif // INCLUDE_FS_FS_H_
