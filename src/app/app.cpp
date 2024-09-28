#include "app.h"
#include "profiler.h"
#include "renderer.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <core/core.h>
#include <core/core/memory.h>
#include <core/os/time.h>
#include <engine/graphics/vulkan/frame.h>
#include <engine/graphics/vulkan/init.h>
#include <engine/utils/config.h>
#include <engine/utils/time.h>
#include <loader/app_loader.h>
#include <type_traits>

using namespace core::enum_helpers;
using math::Quat;
using math::Vec4;

SDL_Gamepad* find_gamepad() {
  int joystick_count;
  SDL_JoystickID* joysticks_ = SDL_GetJoysticks(&joystick_count);
  defer { SDL_free(joysticks_); };

  for (auto joystick : core::storage{usize(joystick_count), joysticks_}.iter()) {
    if (SDL_IsGamepad(joystick)) {
      auto gamepad = SDL_OpenGamepad(joystick);
      LOG_INFO("connected to gamepad %s", SDL_GetGamepadName(gamepad));
      return gamepad;
    }
  }

  LOG_INFO("no gamepad founds");
  return nullptr;
}

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
    }
    break;
  }
  case SDL_EVENT_GAMEPAD_ADDED:
    if (input_state.gamepad == nullptr) {
      input_state.gamepad = SDL_OpenGamepad(ev.gdevice.which);
      LOG_INFO("connected to gamepad %s", SDL_GetGamepadName(input_state.gamepad));
    }
    break;
  case SDL_EVENT_GAMEPAD_REMOVED:
    if (input_state.gamepad && ev.gdevice.which == SDL_GetJoystickID(SDL_GetGamepadJoystick(input_state.gamepad))) {
      LOG_INFO("disconnecting gamepad %s", SDL_GetGamepadName(input_state.gamepad));
      SDL_CloseGamepad(input_state.gamepad);
      input_state.gamepad = nullptr;
      input_state.gamepad = find_gamepad();
    }
    break;
  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    if (input_state.gamepad == nullptr ||
        ev.gbutton.which != SDL_GetJoystickID(SDL_GetGamepadJoystick(input_state.gamepad))) {
      break;
    }
    switch (ev.gaxis.axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
      input_state.x.axisPos = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_LEFTY:
      input_state.z.axisPos = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
      input_state.y.axisNeg = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
      input_state.y.axisPos = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_RIGHTX:
      input_state.yaw.axisPos = -f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_RIGHTY:
      input_state.pitch.axisPos = -f32(ev.gaxis.value) / 32767.f;
      break;
    }
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    if (input_state.gamepad == nullptr ||
        ev.gbutton.which != SDL_GetJoystickID(SDL_GetGamepadJoystick(input_state.gamepad))) {
      break;
    }
    switch (ev.gbutton.button) {
    case SDL_GAMEPAD_BUTTON_START:
      sev |= AppEvent::ReloadApp;
      break;
    }
    break;
  }
  if (ImGui::GetIO().WantCaptureKeyboard) {
    return sev;
  }

  input_state.x.updatePos(ev, SDLK_D, 0, 0);
  input_state.x.updateNeg(ev, SDLK_A, 0, 0);
  input_state.y.updatePos(ev, SDLK_SPACE, 0, 0);
  input_state.y.updateNeg(ev, {}, SDL_KMOD_LSHIFT, SDL_KMOD_LSHIFT);
  input_state.z.updatePos(ev, SDLK_S, 0, 0);
  input_state.z.updateNeg(ev, SDLK_W, 0, 0);
  input_state.pitch.updatePos(ev, SDLK_UP, 0, 0);
  input_state.pitch.updateNeg(ev, SDLK_DOWN, 0, 0);
  input_state.yaw.updatePos(ev, SDLK_LEFT, 0, 0);
  input_state.yaw.updateNeg(ev, SDLK_RIGHT, 0, 0);

  return sev;
}

extern "C" {

EXPORT App* init(AppState* app_state, subsystem::video* video) {
  static App app;
  if (app_state == nullptr) {
    app_state  = core::get_named_allocator(core::AllocatorName::General).allocate<AppState>();
    *app_state = AppState{};
  }
  if (app_state->version != AppState::CUR_VERSION) {
    LOG_ERROR("Resetting app_state: incompatible state version");
    *app_state = AppState{};
  }

  app.arena               = &core::arena_alloc();
  app.video               = video;
  app.state               = app_state;
  app.renderer            = init_renderer(*app.arena, *app.video);
  app.input_state.gamepad = find_gamepad();

  return &app;
}
static_assert(std::is_same_v<decltype(&init), PFN_init>);

EXPORT AppState* uninit(App& app) {
  LOG_DEBUG("Uninit app");

  uninit_renderer(*app.video, *app.renderer);
  core::arena_dealloc(*app.arena);
  return app.state;
}
static_assert(std::is_same_v<decltype(&uninit), PFN_uninit>);

using namespace core::literals;

void update(App& app) {
  utils::config_f32xN("input.speed", app.state->config.speed._coeffs, 3);
  utils::config_f32xN("input.rot_speed", (f32*)&app.state->config.rot_speed, 2);

  auto dt      = utils::get_last_frame_dt().secs();
  auto forward = app.state->camera.rotation.rotate(-Vec4::Z);
  auto sideway = app.state->camera.rotation.rotate(Vec4::X);
  auto upward  = Vec4::Y;

  // clang-format off
  app.state->camera.position += dt * (+ app.state->config.speed.x * app.input_state.x.value() * sideway 
                                      + app.state->config.speed.y * app.input_state.y.value() * upward 
                                      - app.state->config.speed.z * app.input_state.z.value() * forward);
  // clang-format on

  app.state->camera.rotation =
      Quat::from_axis_angle(sideway, app.state->config.rot_speed.pitch * dt * app.input_state.pitch.value()) *
      Quat::from_axis_angle(upward, app.state->config.rot_speed.yaw * dt * app.input_state.yaw.value()) *
      app.state->camera.rotation;
}

void debug_stuff(App& app) {
  auto& appconf = app.state->config;

  utils::config_u64("app_state.version", &app.state->version, true);

  utils::config_bool("timing.wait_timing_target", &appconf.wait_timing_target);
  utils::config_u64("timing.timing_target_usec", &appconf.timing_target_usec);
  if (app.state->config.wait_timing_target) {
    auto frame_report_scope = utils::scope_start("wait timing target"_hs);
    defer { utils::scope_end(frame_report_scope); };

    os::sleep(USEC(appconf.timing_target_usec));
  }

  utils::config_bool("debug.frame_report", &appconf.print_frame_report);
  utils::config_bool("debug.frame_report.full", &appconf.print_frame_report_full);
  if (app.state->config.print_frame_report) {
    auto frame_report_scope = utils::scope_start("frame report"_hs);
    defer { utils::scope_end(frame_report_scope); };

    auto timing_infos = utils::get_last_frame_timing_infos(core::get_named_allocator(core::AllocatorName::Frame));
    if (appconf.print_frame_report_full) {
      for (auto [name, t] : timing_infos.timings.iter()) {
        LOG_BUILDER(core::LogLevel::Debug, push(name).push(" at ").push(t, os::TimeFormat::MMM_UUU_NNN));
      }
    }

    LOG_BUILDER(
        core::LogLevel::Debug,
        push("frame mean: ").push(timing_infos.stats.mean_frame_time, os::TimeFormat::MMM_UUU_NNN)
    );
    LOG_BUILDER(
        core::LogLevel::Debug, push("frame low 95: ").push(timing_infos.stats.low_95, os::TimeFormat::MMM_UUU_NNN)
    );
    LOG_BUILDER(
        core::LogLevel::Debug, push("frame low 99: ").push(timing_infos.stats.low_99, os::TimeFormat::MMM_UUU_NNN)
    );
  }

  utils::config_f32xN("camera.position", app.state->camera.position._coeffs, 3, true);
  utils::config_f32xN("camera.rotation", app.state->camera.rotation.v._coeffs, 4, true);

  static Vec4 input_move_dir = Vec4::Zero;
  input_move_dir             = Vec4{
      app.input_state.x.value(),
      app.input_state.y.value(),
      app.input_state.z.value(),
      0,
  };
  utils::config_f32xN("input.move_dir", input_move_dir._coeffs, 3, true);
}

EXPORT AppEvent frame(App& app) {
  AppEvent sev{};
  SDL_Event ev{};

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  defer { ImGui::EndFrame(); };
  utils::config_new_frame();

  profiling_window();
  debug_stuff(app);

  auto poll_event_scope = utils::scope_start("poll event start"_hs);
  while (SDL_PollEvent(&ev)) {
    sev |= handle_events(ev, app.input_state);
  }
  utils::scope_end(poll_event_scope);

  update(app);

  AppEvent renderev  = render(app.state, *app.video, *app.renderer);
  sev               |= renderev;

  if (any(renderev & AppEvent::SkipRender)) {
    LOG_TRACE("rendering skiped");
    return sev;
  }

  if (any(sev & AppEvent::RebuildRenderer)) {
    LOG_INFO("rebuilding renderer");
    uninit_renderer(*app.video, *app.renderer);
    app.renderer = init_renderer(*app.arena, *app.video);
  }
  if (any(sev & AppEvent::RebuildSwapchain)) {
    subsystem::video_rebuild_swapchain(*app.video);
    swapchain_rebuilt(*app.video, *app.renderer);
  }

  return sev;
}
}
