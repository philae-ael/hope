#ifndef INCLUDE_APP_APP_H_
#define INCLUDE_APP_APP_H_

#include "app/renderer.h"
#include "core/core.h"
#include "core/vulkan/subsystem.h"

struct App {
  core::Arena* arena;
  subsystem::video* video;
  Renderer* renderer;

  AppState* state;
};

struct AppState {
  u32 reload_count = 0;
};

#endif // INCLUDE_APP_APP_H_
