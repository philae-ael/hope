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
struct Renderer;

union SDL_Event;
#define APP_PFNS                                                 \
  PFN(init, void)                                                \
  PFN(uninit, void)                                              \
  PFN(handle_events, AppEvent, SDL_Event&)                       \
  PFN(init_renderer, Renderer*, core::Arena&, subsystem::video&) \
  PFN(render, AppEvent, subsystem::video&, Renderer*)            \
  PFN(swapchain_rebuilt, void, subsystem::video&, Renderer*)     \
  PFN(uninit_renderer, void, subsystem::video&, Renderer*)

#define PFN(name, ret, ...) using CONCAT(PFN_, name) = ret (*)(__VA_ARGS__);
EVAL(APP_PFNS)
#undef PFN

struct App {
  void* handle;
#define PFN(name, ret, ...) CONCAT(PFN_, name) name;
  EVAL(APP_PFNS)
#undef PFN
#if SHARED
  bool need_reload;
#endif
};

#if SHARED
inline core::str8 default_soname = core::str8::from("lib/libapp.so");
#endif

void init_app(App& app);
bool need_reload(App& app);
void reload_app(App& app);

#endif // INCLUDE_SRC_APP_LOADER_H_
