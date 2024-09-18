#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

#include "app_loader.h"
#include "core/debug/time.h"
#include "core/fs/fs.h"
#include "core/os/time.h"
#include <core/vulkan/subsystem.h>

#include <core/core.h>
#include <core/os.h>
#include <cstdlib>

struct Renderer;

using namespace core;
using namespace core::literals;
using namespace core::enum_helpers;

log_entry timed_formatter(void* u, Arena& arena, core::log_entry entry) {
  entry         = log_fancy_formatter(nullptr, arena, entry);
  entry.builder = string_builder{}
                      .push(arena, os::time_monotonic(), os::TimeFormat::MM_SS_MMM)
                      .push(arena, " ")
                      .append(entry.builder);
  return entry;
}

int main(int argc, char* argv[]) {
  auto& ar = arena_alloc();
  setup_crash_handler();
  log_register_global_formatter(log_timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Trace);
#if LINUX
  const char* cwd = realpath(".", nullptr);
  fs::init(core::str8::from(cstr, cwd));
  free((void*)cwd);
#elif WINDOWS
  const char* cwd = _fullpath(nullptr, "../corelib/", 0);
  fs::init(core::str8::from(cstr, cwd));
  free((void*)cwd);

#endif
#ifdef SHARED
  fs::register_path("lib"_s, fs::resolve_path(ar, "build"_s));
#endif

  debug::init();

  SDL_Init(SDL_INIT_GAMEPAD);

  auto video = subsystem::init_video(ar);
  AppPFNs app_pfns;
  load_app(app_pfns);
  App* app = app_pfns.init(ar, nullptr, nullptr, &video);
  LOG_INFO("App fully initialized");

  auto& frame_arena = arena_alloc();
  while (!false) {
    auto frame_ar = frame_arena.make_temp();
    defer { frame_ar.retire(); };

    debug::frame_start();
    defer { debug::frame_end(); };

    auto fs_process_scope = debug::scope_start("fs process"_hs);
    fs::process_modified_file_callbacks();
    debug::scope_end(fs_process_scope);

    auto sev = app_pfns.frame(*frame_ar, *app);
    if (any(sev & AppEvent::Exit)) {
      break;
    }

    if (any(sev & AppEvent::ReloadApp) || need_reload(app_pfns)) {
      LOG_INFO("reloading app");

      auto app_state = app_pfns.uninit(*app);
      reload_app(app_pfns);
      app = app_pfns.init(ar, app, app_state, &video);
      debug::reset();
    }
  }

  LOG_INFO("Exiting...");
  app_pfns.uninit(*app);

  subsystem::uninit_video(video);
  SDL_Quit();
  LOG_INFO("Bye");
}
