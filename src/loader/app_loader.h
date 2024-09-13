#ifndef INCLUDE_SRC_APP_LOADER_H_
#define INCLUDE_SRC_APP_LOADER_H_

#include <core/core.h>
#include <core/vulkan/subsystem.h>

enum class AppEvent : u8 {
  Exit             = 0x1,
  RebuildRenderer  = 0x2,
  RebuildSwapchain = 0x4,
  ReloadApp        = 0x8,
  SkipRender       = 0x10,
};

struct App;
struct AppPFNs;
struct AppState;
struct AppInfo;
struct Renderer;

union SDL_Event;

using PFN_init   = App* (*)(core ::Arena&, App*, AppState*, subsystem ::video*);
using PFN_uninit = AppState* (*)(App&);
using PFN_frame  = AppEvent (*)(core ::Arena&, App&);

struct AppPFNs {
  void* handle;

  PFN_init init;
  PFN_uninit uninit;
  PFN_frame frame;

#if SHARED
  bool need_reload;
#endif
};

#if SHARED
inline core::str8 default_soname = core::str8::from("lib/libapp.so");
#endif

void load_app(AppPFNs& app);
bool need_reload(AppPFNs& app);
void reload_app(AppPFNs& app);

#endif // INCLUDE_SRC_APP_LOADER_H_
