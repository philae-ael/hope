// IWYU pragma: private
#ifndef INCLUDE_OS_FS_H_
#define INCLUDE_OS_FS_H_
#include <core/core.h>

namespace os {
core::str8 getcwd(core::Allocator alloc);
}

#endif // INCLUDE_OS_FS_H_
