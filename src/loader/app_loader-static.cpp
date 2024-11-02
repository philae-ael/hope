#include "app_loader.h"
#include "core/core.h"

struct Renderer;
union SDL_Event;

extern "C" {
App* init(AppState*, subsystem ::video*);
AppState* uninit(App&);
AppEvent process_events(App&);
AppEvent new_frame(App&);
}

void load_app(AppPFNs& app) {
  app = AppPFNs{
      .handle         = nullptr,
      .init           = init,
      .uninit         = uninit,
      .process_events = process_events,
      .new_frame      = new_frame,
  };
}

bool need_reload(AppPFNs& app) {
  return false;
}

void reload_app(AppPFNs& app) {
  LOG_DEBUG("reloading does nothing in static build");
}
