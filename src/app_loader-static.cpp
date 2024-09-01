#include "app_loader.h"
#include "core/core.h"

struct Renderer;
union SDL_Event;

#define PFN(name, ret, ...) ret name(__VA_ARGS__);
extern "C" {
EVAL(APP_PFNS)
}
#undef PFN

App init_app() {
#define PFN(name, ret, ...) .name = name,

  App app{.handle = nullptr, EVAL(APP_PFNS)};
#undef PFN

  return app;
}

bool need_reload(App& app) {
  return false;
}

void reload_app(App& app) {
  app.uninit();
  app = init_app();
}
