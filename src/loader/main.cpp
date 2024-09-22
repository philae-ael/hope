
#include "app_loader.h"

#include <core/core.h>
#include <core/fs/fs.h>
#include <core/os.h>
#include <core/os/time.h>
#include <core/utils/time.h>
#include <core/vulkan/subsystem.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
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
  setup_crash_handler();
  log_register_global_formatter(log_timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Trace);
  utils::init_timing_tracking();
  fs::init();

  auto& ar = arena_alloc();
#ifdef SHARED
  fs::register_path("lib"_s, fs::resolve_path(ar, "build"_s));
#endif

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

    utils::frame_start();
    defer { utils::frame_end(); };

    auto fs_process_scope = utils::scope_start("fs process"_hs);
    fs::process_modified_file_callbacks();
    utils::scope_end(fs_process_scope);

    auto sev = app_pfns.frame(*frame_ar, *app);
    if (any(sev & AppEvent::Exit)) {
      break;
    }

    if (any(sev & AppEvent::ReloadApp) || need_reload(app_pfns)) {
      LOG_INFO("reloading app");

      auto app_state = app_pfns.uninit(*app);
      reload_app(app_pfns);
      app = app_pfns.init(ar, app, app_state, &video);
      utils::reset();
    }
  }

  LOG_INFO("Exiting...");
  app_pfns.uninit(*app);

  subsystem::uninit_video(video);
  SDL_Quit();
  LOG_INFO("Bye");
}
