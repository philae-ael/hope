#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

#include "app_loader.h"
#include "subsystems.h"

#include <core/core.h>
#include <core/os.h>

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
  log_register_global_formatter(timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Trace);

  auto video = subsystem::init_video();
  App app    = init_app();
  Renderer* renderer;
  auto& ar = arena_alloc();

  renderer = app.init_renderer(ar, video);
  LOG_INFO("App fully initialized");

  while (true) {
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
      app.uninit();
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
