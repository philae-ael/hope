#include "app_loader.h"

#include <core/core.h>
#include <core/fs/fs.h>

App init_app_stub() {
#define PFN(name, ret, ...) .name = [](__VA_ARGS__) -> ret { return (ret)(0); },
  LOG_INFO("stub");
  return App{.handle = nullptr, EVAL(APP_PFNS)};
#undef PFN
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
void uninit_app() {
  if (libapp_handle != nullptr) {
    auto pfn_uninit_app = (App(*)())(dlsym(libapp_handle, "uninit"));
    CHECK_DLERROR("can't load symbol uninit");
    pfn_uninit_app();

    dlclose(libapp_handle);
    libapp_handle = nullptr;
    return;
  failed:
    return;
  }
}

App load_app(core::str8 soname_) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  const char* soname = fs::resolve_path(*scratch, soname_).cstring(*scratch);

  App app;
  LOG_DEBUG("loading app at location %s", soname);

  dlerror();
  libapp_handle = dlopen(soname, RTLD_LOCAL | RTLD_NOW);
  app.handle    = libapp_handle;
  if (libapp_handle == nullptr) {
    goto failed;
  }

  #define PFN(name, ret, ...)                                      \
    app.name = (ret(*)(__VA_ARGS__))(dlsym(libapp_handle, #name)); \
    CHECK_DLERROR("can't load symbol " #name);
  EVAL(APP_PFNS)
  #undef PFN

finish:
  app.init();
  app.uninit      = uninit_app;
  app.need_reload = false;
  return app;

failed:
  app = init_app_stub();
  goto finish;
}
#endif

void on_app_need_reload(void* userdata) {
  App& app = *static_cast<App*>(userdata);
  LOG_INFO("app need reload!");
  app.need_reload = true;
}

void init_app(App& app) {
  app = load_app(default_soname);
  fs::register_modified_file_callback(default_soname, on_app_need_reload, &app);
}

bool need_reload(App& app) {
  return app.need_reload;
}

void reload_app(App& app) {
  app.uninit();
  app = load_app(default_soname);
}
