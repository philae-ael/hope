#include "loader/app_loader.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>

#include <core/core.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/init.h>

using namespace core::enum_helpers;
extern "C" {
EXPORT AppEvent handle_events(SDL_Event& ev) {
  using namespace core::enum_helpers;
  // auto& = *static_cast<>(userdata);

  AppEvent system_event{};
  switch (ev.type) {
  case SDL_QUIT: {
    system_event |= AppEvent::Exit;
    break;
  }
  case SDL_WINDOWEVENT: {
    switch (ev.window.event) {
    case SDL_WINDOWEVENT_CLOSE:
      system_event |= AppEvent::Exit;
      break;
    }

    break;
  }
  case SDL_KEYDOWN: {
    switch (ev.key.keysym.sym) {
    case (SDLK_q): {
      system_event |= AppEvent::Exit;
      break;
    }
    case (SDLK_r): {
      system_event |= AppEvent::RebuildRenderer;
      break;
    }
    }
    break;
  }
  }
  return system_event;
}

EXPORT void uninit() {}
}
