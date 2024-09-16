#ifndef INCLUDE_APP_APP_H_
#define INCLUDE_APP_APP_H_

#include "app/camera.h"
#include "app/renderer.h"
#include "core/core.h"
#include "core/vulkan/subsystem.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

struct Axis {
  bool neg;
  bool pos;
  f32 value() {
    return -1.f * neg + +1 * pos;
  }
  void updateNeg(SDL_Event& ev, SDL_Keycode key, SDL_Keymod modmask, SDL_Keymod mod) {
    if (shoulHandle(ev, key)) {
      neg = (ev.type == SDL_EVENT_KEY_DOWN) && ((ev.key.mod & modmask) == mod);
    }
  }
  void updatePos(SDL_Event& ev, SDL_Keycode key, SDL_Keymod modmask, SDL_Keymod mod) {
    if (shoulHandle(ev, key)) {
      pos = (ev.type == SDL_EVENT_KEY_DOWN) && ((ev.key.mod & modmask) == mod);
    }
  }

  bool shoulHandle(SDL_Event& ev, SDL_Keycode key) {
    if (ev.type != SDL_EVENT_KEY_DOWN && ev.type != SDL_EVENT_KEY_UP) {
      return false;
    }
    if (ev.key.key != key) {
      return false;
    }
    return true;
  }
};

struct InputState {
  Axis x;
  Axis y;
  Axis z;

  f32 pitch; // x
  f32 yaw;   // y
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
      .hfov = DEGREE(60),
      .near = 0.1f,
      .far  = 5,
  };
};

#endif // INCLUDE_APP_APP_H_
