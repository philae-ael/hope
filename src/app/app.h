#ifndef INCLUDE_APP_APP_H_
#define INCLUDE_APP_APP_H_

#include "app/camera.h"
#include "app/renderer.h"
#include "core/core.h"
#include "core/vulkan/subsystem.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>

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
  AppState* state;
};

struct AppState {
  u32 reload_count = 0;
  Camera camera{
      .hfov     = DEGREE(60),
      .near     = 0.1f,
      .far      = 5,
      .position = 1.5f * math::Vec4::Z,
      .rotation = math::Quat::Id,
  };
};

#endif // INCLUDE_APP_APP_H_
