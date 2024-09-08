#include "app_loader.h"

#include "core/core.h"
#include "core/fs/fs.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <system_error>
#include <thread>

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

std::chrono::time_point<std::chrono::file_clock> last_ftime;
App init_app(core::str8 soname_) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  const char* soname = fs::resolve_path(*scratch, soname_).cstring(*scratch);

  using namespace std::chrono_literals;
  const u32 maxretry = 5;
  u32 retry          = 0;

  std::error_code ec;
  App app;
  last_ftime = std::filesystem::last_write_time(soname, ec);
  if (ec) {
    LOG_WARNING("can't check write time of file %s", soname);
    goto failed;
  }
do_retry:
  LOG_DEBUG("loading app at location %s", soname);

  dlerror();
  libapp_handle = dlopen(soname, RTLD_LOCAL | RTLD_NOW);
  app.handle    = libapp_handle;
  if (libapp_handle == nullptr) {
    if (retry < maxretry) {
      retry++;
      LOG_WARNING(
          "error while loading %s retrying... %d/%d: %s", soname, retry, maxretry, dlerror()
      );
      std::this_thread::sleep_for(100ms);
      goto do_retry;
    } else {
      CHECK_DLERROR("Can't load so %s", soname);
    }
  }

  #define PFN(name, ret, ...)                                      \
    app.name = (ret(*)(__VA_ARGS__))(dlsym(libapp_handle, #name)); \
    CHECK_DLERROR("can't load symbol " #name);
  EVAL(APP_PFNS)
  #undef PFN
  app.init();
  app.uninit = uninit_app;
  return app;

failed:
  return init_app_stub();
}
#endif

bool need_reload(App& app) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  const char* soname = ::fs::resolve_path(*scratch, default_soname).cstring(*scratch);

  using namespace std::chrono_literals;
  std::error_code ec1, ec2;

  auto ftime = std::filesystem::last_write_time(soname, ec1);
  auto size  = std::filesystem::file_size(soname, ec2);
  if (!ec1 && !ec2 && ftime > last_ftime && size > 0 &&
      std::chrono::file_clock::now() - ftime >= 100ms) {
    LOG_DEBUG("need reload!");
    return true;
  }
  return false;
}

void reload_app(App& app) {
  app.uninit();
  app = init_app(default_soname);
}
