#include "app_loader.h"

#include "core/core.h"
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

void uninit_app(App& app) {
  if (app.handle != nullptr) {
    auto pfn_uninit_app = (App(*)())(dlsym(app.handle, "uninit"));
    CHECK_DLERROR("can't load symbol uninit");
    pfn_uninit_app();

    dlclose(app.handle);
    return;
  failed:
    app = init_app_stub();
  }
}

std::chrono::time_point<std::chrono::file_clock> last_ftime;
App init_app(const char* soname) {
  using namespace std::chrono_literals;
  const u32 maxretry = 5;
  u32 retry          = 0;

  last_ftime         = std::filesystem::last_write_time(soname);
do_retry:
  LOG_DEBUG("loading app at location %s", soname);

  dlerror();
  void* libapp_handle = dlopen(soname, RTLD_LOCAL | RTLD_NOW);
  App app{libapp_handle};
  if (libapp_handle == nullptr) {
    if (retry < maxretry) {
      retry++;
      LOG_WARNING(
          "error while loading %s retrying... %d/%d: %s", default_soname, retry, maxretry, dlerror()
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
  return app;

failed:
  return init_app_stub();
}
#endif

bool need_reload(App& app) {
  using namespace std::chrono_literals;
  std::error_code ec1, ec2;
  auto ftime = std::filesystem::last_write_time(default_soname, ec1);
  auto size  = std::filesystem::file_size(default_soname, ec2);
  if (!ec1 && !ec2 && ftime > last_ftime && size > 0 &&
      std::chrono::file_clock::now() - ftime >= 100ms) {
    LOG_DEBUG("need reload!");
    return true;
  }
  return false;
}

void reload_app(App& app) {
  using namespace std::chrono_literals;
  namespace fs       = std::filesystem;

  static usize count = 0;
  auto ar            = core::scratch_get();
  defer { ar.retire(); };

  uninit_app(app);

  core::string_builder sb{};
  sb.pushf(*ar, "./libapp-%zu.so", count++);
  const char* path = sb.commit(*ar).cstring(*ar);

  fs::copy_file(default_soname, path);
  app = init_app(path);
  fs::remove(path);
}
