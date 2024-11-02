#ifndef INCLUDE_SRC_APP_LOADER_H_
#define INCLUDE_SRC_APP_LOADER_H_

#include <core/core.h>
#include <engine/graphics/subsystem.h>

enum class AppEvent : u8 {
  Exit             = 0x1,
  RebuildRenderer  = 0x2,
  RebuildSwapchain = 0x4,
  ReloadApp        = 0x8,
  NewFrame         = 0x10,
  SkipRender       = 0x20,
};

struct App;
struct AppPFNs;
struct AppState;
struct AppInfo;
struct Renderer;

union SDL_Event;

using PFN_init           = App* (*)(AppState*, subsystem ::video*);
using PFN_uninit         = AppState* (*)(App&);
using PFN_process_events = AppEvent (*)(App&);
using PFN_new_frame      = AppEvent (*)(App&);

struct AppPFNs {
  void* handle;

  PFN_init init;
  PFN_uninit uninit;
  PFN_process_events process_events;
  PFN_new_frame new_frame;

#if SHARED
  bool need_reload;
#endif
};

#if SHARED
inline core::str8 default_soname = core::str8::from("/lib/libapp.so");
#endif

void load_app(AppPFNs& app);
bool need_reload(AppPFNs& app);
void reload_app(AppPFNs& app);

#endif // INCLUDE_SRC_APP_LOADER_H_
