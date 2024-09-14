#include "app_loader.h"

#include <SDL3/SDL_events.h>
#include <core/core.h>
#include <core/fs/fs.h>

AppPFNs init_app_stub() {
  LOG_INFO("stub");
  return AppPFNs{
      .handle = nullptr,
      .init = [](core ::Arena&, App*, AppState*, subsystem ::video*) -> App* { return (App*)(0); },
      .uninit = [](App&) -> AppState* { return (AppState*)(0); },
      .frame  = [](core ::Arena&, App&) -> AppEvent {
        using namespace core::enum_helpers;
        AppEvent sev{};
        SDL_Event ev{};
        while (SDL_PollEvent(&ev)) {
          switch (ev.type) {
          case SDL_EVENT_QUIT: {
            sev |= AppEvent::Exit;
            break;
          }
          case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            sev |= AppEvent::Exit;
            break;
          }
          case SDL_EVENT_KEY_DOWN: {
            switch (ev.key.key) {
            case SDLK_Q: {
              sev |= AppEvent::Exit;
              break;
            }
            case SDLK_ESCAPE: {
              sev |= AppEvent::ReloadApp;
              break;
            }
            }
            break;
          }
          }
        }
        return sev;
      },
  };
}

#if LINUX
  #include <dlfcn.h>
  #define CHECK_DLERROR(msg, ...)                             \
    do {                                                      \
      const char* err = dlerror();                            \
      if (err != nullptr) {                                   \
        LOG_ERROR(msg ": %s" __VA_OPT__(, __VA_ARGS__), err); \
        goto failed;                                          \
      }                                                       \
    } while (0)

void* libapp_handle = nullptr;
AppState* uninit_app(App& app) {
  LOG_INFO("uninit app");
  AppState* app_state = nullptr;

  if (libapp_handle != nullptr) {
    auto pfn_uninit_app = (AppState * (*)(App&))(dlsym(libapp_handle, "uninit"));
    CHECK_DLERROR("can't load symbol uninit");
    app_state = pfn_uninit_app(app);

    dlclose(libapp_handle);
    libapp_handle = nullptr;
  failed:
  }
  return app_state;
}

AppPFNs load_app(core::str8 soname_) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  const char* soname = fs::resolve_path(*scratch, soname_).cstring(*scratch);

  AppPFNs app;
  LOG_DEBUG("loading app at location %s", soname);

  dlerror();
  libapp_handle = dlopen(soname, RTLD_LOCAL | RTLD_NOW);
  app.handle    = libapp_handle;
  CHECK_DLERROR("can't open %s", soname);

  app.init = (PFN_init)dlsym(libapp_handle, "init");
  CHECK_DLERROR("can't load init");

  app.uninit = (PFN_uninit)dlsym(libapp_handle, "uninit");
  CHECK_DLERROR("can't open uninit");

  app.frame = (PFN_frame)dlsym(libapp_handle, "frame");
  CHECK_DLERROR("can't open frame");

finish:
  app.uninit      = uninit_app;
  app.need_reload = false;
  return app;

failed:
  app = init_app_stub();
  goto finish;
}
#endif

void on_app_need_reload(void* userdata) {
  AppPFNs& app = *static_cast<AppPFNs*>(userdata);
  LOG_INFO("app need reload!");
  app.need_reload = true;
}

void load_app(AppPFNs& app) {
  app = load_app(default_soname);
  fs::register_modified_file_callback(default_soname, on_app_need_reload, &app);
}

bool need_reload(AppPFNs& app) {
  return app.need_reload;
}

void reload_app(AppPFNs& app) {
  app = load_app(default_soname);
}
