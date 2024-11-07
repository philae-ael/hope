#ifndef INCLUDE_APP_APP_H_
#define INCLUDE_APP_APP_H_

#include "app/camera.h"
#include "app/grid_renderer.h"
#include "app/renderer.h"
#include "core/core.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>
#include <engine/graphics/subsystem.h>

namespace core {
inline f32 clamp_positive(f32 value) {
  return MAX(0, value);
}
inline f32 sign(f32 value) {
  return value >= 0 ? 1.0 : -1.0;
}
inline f32 abs(f32 value) {
  return sign(value) * value;
}
} // namespace core

#define GAMEPAD_DEAD_ZONE 0.1

struct Axis {
  bool neg;
  bool pos;
  f32 axisPos;
  f32 axisNeg;
  f32 value() const {
    auto v = axisPos - axisNeg;
    return core::sign(v) * core::clamp_positive(core::abs(v) - 0.1f) + -1.f * neg + +1 * pos;
  }
  void updateNeg(SDL_Event& ev, core::Maybe<SDL_Keycode> key, SDL_Keymod modmask, SDL_Keymod mod) {
    if (shouldHandle(ev, key)) {
      neg = (key.is_none() || ev.type == SDL_EVENT_KEY_DOWN) && ((ev.key.mod & modmask) == mod);
    }
  }
  void updatePos(SDL_Event& ev, core::Maybe<SDL_Keycode> key, SDL_Keymod modmask, SDL_Keymod mod) {
    if (shouldHandle(ev, key)) {
      pos = (key.is_none() || ev.type == SDL_EVENT_KEY_DOWN) && ((ev.key.mod & modmask) == mod);
    }
  }

  bool shouldHandle(SDL_Event& ev, core::Maybe<SDL_Keycode> key) {
    if (ev.type != SDL_EVENT_KEY_DOWN && ev.type != SDL_EVENT_KEY_UP) {
      return false;
    }
    if (key.is_some() && ev.key.key != *key) {
      return false;
    }
    return true;
  }
};

struct InputState {
  SDL_Gamepad* gamepad;
  Axis x;
  Axis y;
  Axis z;

  Axis pitch;
  Axis yaw;
};

struct App {
  core::Arena* arena;
  subsystem::video* video;
  Renderer* renderer;
  InputState input_state{};
  AppState* state = nullptr;
};

struct AppState {
  // WARN: BUMP THIS ON MODIFICATION OF STATE
  static const usize CUR_VERSION = 7;
  usize version                  = CUR_VERSION;
  Camera camera{
      .hfov     = DEGREE(60),
      .near     = 1.f,
      .far      = 50.f,
      .position = 1.5f * math::Vec4::Z,
      .rotation = math::Quat::Id,
  };
  struct {

    // === DEBUG ===
    bool wait_timing_target      = false;
    bool print_frame_report      = false;
    bool print_frame_report_full = true;
    u64 timing_target_usec       = 5500;

    GridConfig grid{};

    // === MOVEMENT ===

    math::Vec4 speed{7, 3, 0, 0};
    struct {
      f32 pitch = 2;
      f32 yaw   = 2;
    } rot_speed;

  } config;
};

#endif // INCLUDE_APP_APP_H_
