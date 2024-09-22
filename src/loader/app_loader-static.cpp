#include "app_loader.h"
#include "core/core.h"

struct Renderer;
union SDL_Event;

extern "C" {
App* init(core::Allocator alloc, App*, AppState*, subsystem ::video*);
AppState* uninit(App&);
AppEvent frame(core ::Arena&, App&);
}

void load_app(AppPFNs& app) {
  app = AppPFNs{
      .handle = nullptr,
      .init   = init,
      .uninit = uninit,
      .frame  = frame,
  };
}

bool need_reload(AppPFNs& app) {
  return false;
}

void reload_app(AppPFNs& app) {
  LOG_DEBUG("reloading does nothing in static build");
}
