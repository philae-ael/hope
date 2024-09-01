#include "app_loader.h"

#include "core/core.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <system_error>
#include <thread>

#if LINUX
  #include <dlfcn.h>
  #define CHECK_DLERROR(msg, ...)                             \
    do {                                                      \
      const char* err = dlerror();                            \
      if (err != nullptr) {                                   \
        LOG_ERROR(msg ": %s" __VA_OPT__(, __VA_ARGS__), err); \
        exit(1);                                              \
      }                                                       \
    } while (0)

void uninit_app(App& app) {
  if (app.handle != nullptr) {
    auto pfn_uninit_app = (App(*)())(dlsym(app.handle, "uninit"));
    CHECK_DLERROR("can't load symbol uninit");
    pfn_uninit_app();

    dlclose(app.handle);
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
  if (libapp_handle == nullptr) {
    if (retry < maxretry) {
      retry++;
      LOG_WARNING(
          "error while loading %s retrying... %d/%d: %s", default_soname, retry, maxretry, dlerror()
      );
      std::this_thread::sleep_for(5ms);
      goto do_retry;
    }

    CHECK_DLERROR("Can't load so %s", soname);
  }

  App app{libapp_handle};

  #define PFN(name, ret, ...)                                      \
    app.name = (ret(*)(__VA_ARGS__))(dlsym(libapp_handle, #name)); \
    CHECK_DLERROR("can't load symbol " #name);
  EVAL(APP_PFNS)
  #undef PFN

  return app;
}
#endif

bool need_reload(App& app) {
  using namespace std::chrono_literals;
  const u32 maxretry = 5;
  u32 retry          = 0;

do_retry:
  ASSERT(retry < maxretry);
  retry++;

  std::error_code ec;
  auto ftime = std::filesystem::last_write_time(default_soname, ec);
  if (ec.value() == (int)std::errc::no_such_file_or_directory) {
    LOG_WARNING(
        "error while getting time for file %s retrying... %d/%d: no such file of directory",
        default_soname, retry, maxretry
    );
    std::this_thread::sleep_for(5ms);
    goto do_retry;
  }

  if (ftime > last_ftime) {
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

  // wait end of file write... maybe enough...maybe not
  std::error_code ec;
  usize retry{};
  while ((fs::file_size(path, ec) == 0 || ec) && retry++ < 5) {
    std::this_thread::sleep_for(100ms);
  }
  fs::copy_file(default_soname, path);
  app = init_app(path);
  fs::remove(path);
}
