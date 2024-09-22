#define _CRT_SECURE_NO_WARNINGS

#include "../fs.h"
#include <core/core.h>

#include <cerrno>
#include <cstring>
#include <direct.h>

namespace os {
EXPORT core::str8 getcwd(core::Allocator alloc) {
  auto buf = alloc.allocate_array<u8>(1024, "getcwd");

  const usize max_size = KB(16);

  while (_getcwd((char*)buf.data, buf.size) == NULL) {
    switch (errno) {
    case ERANGE: {
      ASSERTM(2 * buf.size < max_size, "cwd seems to be too big, max length is set to %lu KB", max_size / 1024 / 1024);
      ASSERTM(
          alloc.try_resize(buf.data, buf.size, 2 * buf.size, "getcwd"),
          "can't grow buffer, i could allocated another one but i don't wanna"
      );
      buf.size = 2 * buf.size;
      break;
    }
    default:
      core::panic("error while getting cwd: %s", strerror(errno));
    }
  }

  auto len = strlen((const char*)buf.data);
  alloc.try_resize(buf.data, buf.size, len);
  return {len, buf.data};
}
} // namespace os
