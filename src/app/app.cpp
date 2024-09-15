#include "app.h"
#include "app/renderer.h"
#include "core/core/memory.h"
#include "loader/app_loader.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>

#include <core/core.h>
#include <core/debug/config.h>
#include <core/debug/time.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/init.h>

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

using namespace core::enum_helpers;
AppEvent handle_events(SDL_Event& ev) {
  using namespace core::enum_helpers;

  ImGui_ImplSDL3_ProcessEvent(&ev);
  AppEvent sev{};
  switch (ev.type) {
  case SDL_EVENT_QUIT: {
    sev |= AppEvent::Exit;
    break;
  }
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
    sev |= AppEvent::Exit;
    break;
  }
  case SDL_EVENT_KEY_DOWN: {
    if (ImGui::GetIO().WantCaptureKeyboard) {
      break;
    }
    switch (ev.key.key) {
    case SDLK_Q: {
      sev |= AppEvent::Exit;
      break;
    }
    case SDLK_R: {
      sev |= AppEvent::RebuildRenderer;
      break;
    }
    case SDLK_ESCAPE: {
      sev |= AppEvent::ReloadApp;
      break;
    }
    }
    break;
  }
  }
  return sev;
}

extern "C" {
EXPORT App* init(core::Arena& arena, App* app, AppState* app_state, subsystem::video* video) {
  LOG_DEBUG("Init app");
  if (app == nullptr) {
    app = new (arena.allocate<App>()) App{};
  }
  app->arena = &core::arena_alloc();
  app->video = video;

  if (app_state == nullptr) {
    LOG_DEBUG("No app state, creating app state");
    app_state  = arena.allocate<AppState>();
    app->state = new (app_state) AppState{};
  } else {
    app->state                = app_state;
    app->state->reload_count += 1;
    LOG_DEBUG("Reusing app state, reloaded %d times", app->state->reload_count);
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io     = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplSDL3_InitForOther(video->window);
  app->renderer = init_renderer(*app->arena, *app->video);

  return app;
}

EXPORT AppState* uninit(App& app) {
  LOG_DEBUG("Uninit app");

  uninit_renderer(*app.video, *app.renderer);
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  core::arena_dealloc(*app.arena);
  return app.state;
}

using namespace core::literals;
EXPORT AppEvent frame(core::Arena& frame_arena, App& app) {
  AppEvent sev{};
  SDL_Event ev{};

handle_events: {
  auto poll_event_scope = debug::scope_start("poll event start"_hs);
  defer { debug::scope_end(poll_event_scope); };

  while (SDL_PollEvent(&ev)) {
    sev |= handle_events(ev);
  }
}

  AppEvent renderev = render(*app.video, *app.renderer);
  if (any(renderev & AppEvent::SkipRender)) {
    LOG_TRACE("skip");
    goto handle_events;
  }

  sev |= renderev;

  if (any(sev & AppEvent::RebuildRenderer)) {
    LOG_INFO("rebuilding renderer");
    uninit_renderer(*app.video, *app.renderer);
    app.renderer = init_renderer(*app.arena, *app.video);
  }
  if (any(sev & AppEvent::RebuildSwapchain)) {
    subsystem::video_rebuild_swapchain(*app.video);
    swapchain_rebuilt(*app.video, *app.renderer);
  }

  static bool print_frame_report = false;
  debug::config_bool("Print frame report", &print_frame_report);
  if (print_frame_report) {
    auto frame_report_scope = debug::scope_start("frame report"_hs);
    defer { debug::scope_end(frame_report_scope); };

    auto timing_infos = debug::get_last_frame_timing_infos(frame_arena);
    for (auto [name, t] : timing_infos.timings.iter()) {
      LOG_BUILDER(
          core::LogLevel::Trace, push(name).push(" at ").push(t, os::TimeFormat::MMM_UUU_NNN)
      );
    }
  }

  return sev;
}
}
