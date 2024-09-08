#ifndef INCLUDE_FS_FS_H_
#define INCLUDE_FS_FS_H_

#include <core/core.h>

namespace fs {
void init(core::str8 path);
void register_path(core::str8 path, core::str8 target);
core::str8 resolve_path(core::Arena& ar, core::str8 path);
core::storage<u8> read_all(core::Arena& ar, core::str8 path);
} // namespace fs

#endif // INCLUDE_FS_FS_H_
