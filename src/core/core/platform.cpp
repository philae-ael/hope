#include <core/core.h>

using namespace core::literals;

namespace core {
str8 to_str8(Os os) {
  str8 data[]{
      "windows"_s,
      "linux"_s,
  };
  return data[(usize)os];
}

template <>
Maybe<Os> from_hstr8(hstr8 h) {
  switch (h.hash) {
  case "windows"_h:
    return Os::Windows;
  case "linux"_h:
    return Os::Linux;
  }
  return {};
}
} // namespace core
