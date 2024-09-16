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

AppEvent handle_events(SDL_Event& ev, InputState& input_state) {
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
  case SDL_EVENT_KEY_UP: {
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
    case SDLK_RIGHT:
    case SDLK_LEFT: {
      input_state.yaw = 0.0f;
      break;
    }
    case SDLK_UP:
    case SDLK_DOWN: {
      input_state.pitch = 0.0f;
      break;
    }
    }
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
    case SDLK_LEFT: {
      input_state.yaw = +1.0f;
      break;
    }
    case SDLK_RIGHT: {
      input_state.yaw = -1.0f;
      break;
    }
    case SDLK_UP: {
      input_state.pitch = +1.0f;
      break;
    }
    case SDLK_DOWN: {
      input_state.pitch = -1.0f;
      break;
    }
    }
    break;
  }
  }
  if (ImGui::GetIO().WantCaptureKeyboard) {
    return sev;
  }

  input_state.x.updatePos(ev, SDLK_D, 0, 0);
  input_state.x.updateNeg(ev, SDLK_A, 0, 0);
  input_state.y.updatePos(ev, SDLK_SPACE, SDL_KMOD_LSHIFT, 0);
  input_state.y.updateNeg(ev, SDLK_SPACE, SDL_KMOD_LSHIFT, SDL_KMOD_LSHIFT);
  input_state.z.updatePos(ev, SDLK_S, 0, 0);
  input_state.z.updateNeg(ev, SDLK_W, 0, 0);

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
  app->renderer              = init_renderer(*app->arena, *app->video);

  app_state->camera.position = 1.5f * core::Vec4::Z;

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
    sev |= handle_events(ev, app.input_state);
  }
}

  auto dt                    = debug::get_last_frame_dt().secs();
  auto forward               = app.state->camera.rotation.rotate(-core::Vec4::Z);
  auto sideway               = app.state->camera.rotation.rotate(core::Vec4::X);
  auto upward                = core::Vec4::Y;

  // clang-format off
  app.state->camera.position += dt * (+ app.input_state.x.value() * sideway 
                                      + app.input_state.y.value() * upward 
                                      - app.input_state.z.value() * forward);
  // clang-format on

  app.state->camera.rotation = core::Quat::from_axis_angle(sideway, dt * app.input_state.pitch) *
                               core::Quat::from_axis_angle(upward, dt * app.input_state.yaw) *
                               app.state->camera.rotation;

  AppEvent renderev = render(app.state, *app.video, *app.renderer);
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
