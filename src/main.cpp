#include "containers/vec.h"
#include "core/core.h"
#include "core/debug.h"
#include "core/log.h"
#include "os/os.h"

#include "vulkan/init.h"

using namespace core;
using namespace core::literals;

log_entry timed_formatter(void* u, arena& arena, core::log_entry entry) {
  entry         = log_fancy_formatter(nullptr, arena, entry);
  entry.builder = string_builder{}
                      .push(arena, os::time_monotonic(), os::TimeFormat::MM_SS_MMM_UUU_NNN)
                      .push(arena, " ")
                      .append(entry.builder);
  return entry;
}

int main(int argc, char* argv[]) {
  setup_crash_handler();
  log_register_global_formatter(log_fancy_formatter, nullptr);
  log_set_global_level(core::LogLevel::Debug);

  vk::init();

  auto s = scratch_get();
  defer { s.retire(); };

  const char* layers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  vk::instance instance =
      vk::create_default_instance(*s, vec{layers}, {}).expect("can't create instance");

  vk::destroy_instance(instance);
}
