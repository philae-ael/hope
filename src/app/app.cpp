#include "backends/imgui_impl_sdl3.h"
#include "imgui.h"
#include "loader/app_loader.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>

#include <core/core.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/init.h>

using namespace core::enum_helpers;
extern "C" {
EXPORT AppEvent handle_events(SDL_Event& ev) {
  using namespace core::enum_helpers;
  // auto& = *static_cast<>(userdata);

  ImGui_ImplSDL3_ProcessEvent(&ev);
  AppEvent system_event{};
  switch (ev.type) {
  case SDL_EVENT_QUIT: {
    system_event |= AppEvent::Exit;
    break;
  }
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
    system_event |= AppEvent::Exit;
  }
  case SDL_EVENT_KEY_DOWN: {
    if (ImGui::GetIO().WantCaptureKeyboard) {
      break;
    }
    switch (ev.key.key) {
    case (SDLK_Q): {
      system_event |= AppEvent::Exit;
      break;
    }
    case (SDLK_R): {
      system_event |= AppEvent::RebuildRenderer;
      break;
    }
    }
    break;
  }
  }
  return system_event;
}

EXPORT void init() {
  LOG_DEBUG("Init app");
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io     = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}
EXPORT void uninit() {
  LOG_DEBUG("Uninit app");
  ImGui::DestroyContext();
}
}
