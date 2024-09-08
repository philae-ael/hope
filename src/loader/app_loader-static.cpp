#include "app_loader.h"
#include "core/core.h"

struct Renderer;
union SDL_Event;

#define PFN(name, ret, ...) ret name(__VA_ARGS__);
extern "C" {
EVAL(APP_PFNS)
}
#undef PFN

void init_app(App& app) {
#define PFN(name, ret, ...) .name = name,
  app                             = App{.handle = nullptr, EVAL(APP_PFNS)};
#undef PFN

  app.init();
}

bool need_reload(App& app) {
  return false;
}

void reload_app(App& app) {
  app.uninit();
  init_app(app);
}
