#ifndef INCLUDE_FS_FS_H_
#define INCLUDE_FS_FS_H_

#include <core/core.h>

namespace fs {

using virtualpath = core::str8;
using realpath    = core::str8;

void init();
void mount(virtualpath path, realpath target);
core::Maybe<realpath> resolve_path(core::Allocator alloc, virtualpath);
core::storage<u8> read_all(core::Allocator alloc, core::str8 path);

using on_file_modified_t      = void (*)(void*);
using on_file_modified_handle = core::handle_t<on_file_modified_t>;
on_file_modified_handle register_modified_file_callback(virtualpath vpath, on_file_modified_t callback, void* userdata);
void unregister_modified_file_callback(on_file_modified_handle h);

void process_modified_file_callbacks();

} // namespace fs

#endif // INCLUDE_FS_FS_H_
