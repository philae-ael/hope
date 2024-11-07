
#include "app_loader.h"
#include "core/core/memory.h"
#include "core/os/fs.h"

#include <backends/imgui_impl_sdl3.h>
#include <core/core.h>
#include <core/fs/fs.h>
#include <core/os.h>
#include <core/os/time.h>
#include <engine/graphics/subsystem.h>
#include <engine/utils/config.h>
#include <engine/utils/time.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <imgui.h>
#include <uv.h>

struct Renderer;

using namespace core;
using namespace core::literals;
using namespace core::enum_helpers;

log_entry timed_formatter(void* u, Allocator alloc, core::log_entry entry) {
  entry         = log_fancy_formatter(nullptr, alloc, entry);
  entry.builder = string_builder{}
                      .push(alloc, os::time_monotonic(), os::TimeFormat::MM_SS_MMM)
                      .push(alloc, " ")
                      .append(entry.builder);
  return entry;
}

void mount_paths() {
  auto scratch = core::scratch_get();

  auto cwd = os::getcwd(scratch);
  // fs::mount("/"_s, cwd);

  auto assetdir = core::join(scratch, PATH_SEPARATOR_S, cwd, "assets");
  fs::mount("/assets"_s, assetdir);
  auto shaderdir = core::join(scratch, PATH_SEPARATOR_S, assetdir, "shaders-compiled");
  fs::mount("/assets/shaders"_s, shaderdir);

#ifdef SHARED
  auto libdir = core::join(scratch, "/"_s, cwd, "build");
  fs::mount("/lib"_s, libdir);
#endif
}

int main(int argc, char* argv[]) {
  /// === Env setup and globals initializations  ===
  setup_crash_handler();
  log_register_global_formatter(log_timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Trace);

  uv_loop_init(uv_default_loop());
  fs::init(uv_default_loop());
  mount_paths();

  utils::timings_init();

  auto global_alloc = core::get_named_allocator(core::AllocatorName::General);

  /// === Load App ===

  AppPFNs app_pfns;
  load_app(app_pfns);

  /// === App init ===

  SDL_Init(SDL_INIT_GAMEPAD);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io     = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  auto video = subsystem::init_video(global_alloc);
  ImGui_ImplSDL3_InitForOther(video.window);

  App* app = app_pfns.init(nullptr, &video);
  LOG_INFO("App fully initialized");

  /// === Main loop ===
  /// Note: frame start and end are not directly dictated by the loop
  while (!false) {
    /// === Frame Callbacks ===

    // Note: lib uv is not thread safe...
    // TODO: Use a circular buffer to send data to libuv, in another thread
    uv_run(uv_default_loop(), UV_RUN_NOWAIT);

    /// === Frame Udpate ===
    auto sev = app_pfns.process_events(*app);

    /// === Handle system event ===

    if (any(sev & AppEvent::NewFrame)) {
      sev |= app_pfns.new_frame(*app);
    }

    if (any(sev & AppEvent::Exit)) {
      break;
    }

    if (any(sev & AppEvent::ReloadApp) || need_reload(app_pfns)) {
      LOG_INFO("reloading app");

      auto app_state = app_pfns.uninit(*app);
      reload_app(app_pfns);
      app = app_pfns.init(app_state, &video);
      utils::timings_reset();
      utils::config_reset();
    }
  }

  /// === Cleanup ===

  LOG_INFO("Exiting...");
  app_pfns.uninit(*app);

  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  subsystem::uninit_video(video);
  SDL_Quit();

  uv_loop_close(uv_default_loop());

  LOG_INFO("Bye");
}
