#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

#include "app_loader.h"
#include "core/fs/fs.h"
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
  log_register_global_formatter(timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Trace);
  {
    const char* cwd = realpath(".", nullptr);
    fs::init(core::str8::from(cstr, cwd));
    free((void*)cwd);
  }
#ifdef SHARED
  fs::register_path("lib"_s, fs::resolve_path(ar, "build"_s));
#endif

  SDL_Init(SDL_INIT_GAMEPAD);

  auto video = subsystem::init_video(ar);
  App app;
  init_app(app);
  Renderer* renderer;

  renderer = app.init_renderer(ar, video);
  LOG_INFO("App fully initialized");

  while (true) {
    fs::process_modified_file_callbacks();

    AppEvent sev{};
    SDL_Event ev{};
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT) {
        sev |= AppEvent::Exit;
      }
      if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
        sev |= AppEvent::ReloadApp;
      }
      sev |= app.handle_events(ev);
    }
    sev |= app.render(video, renderer);

    if (any(sev & AppEvent::Exit)) {
      break;
    }
    if (any(sev & AppEvent::RebuildRenderer)) {
      LOG_INFO("rebuilding renderer");
      app.uninit_renderer(video, renderer);
      renderer = app.init_renderer(ar, video);
    }
    if (any(sev & AppEvent::RebuildSwapchain)) {
      subsystem::video_rebuild_swapchain(video);
      app.swapchain_rebuilt(video, renderer);
    }

    if (any(sev & AppEvent::ReloadApp) || need_reload(app)) {
      LOG_INFO("reloading app");

      app.uninit_renderer(video, renderer);
      reload_app(app);
      subsystem::video_rebuild_swapchain(video);
      renderer = app.init_renderer(ar, video);
    }
  }

  LOG_INFO("Exiting...");
  app.uninit_renderer(video, renderer);
  app.uninit();

  subsystem::uninit_video(video);
  SDL_Quit();
  LOG_INFO("Bye");
}
