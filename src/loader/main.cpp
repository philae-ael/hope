
#include "app_loader.h"
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
#include <cstdlib>
#include <imgui.h>

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

int main(int argc, char* argv[]) {
  /// === Env setup and globals initializations  ===
  setup_crash_handler();
  log_register_global_formatter(log_timed_formatter, nullptr);
  log_set_global_level(core::LogLevel::Trace);
  utils::timings_init();
  fs::init();

  auto global_alloc = core::get_named_allocator(core::AllocatorName::General);
  {
    auto cwd = os::getcwd(global_alloc);
    // fs::mount("/"_s, cwd);

    auto assetdir = core::join(global_alloc, "/"_s, cwd, "assets");
    fs::mount("/assets"_s, assetdir);
    global_alloc.deallocate(assetdir.data);

#ifdef SHARED
    auto libdir = core::join(global_alloc, "/"_s, cwd, "build");
    fs::mount("/lib"_s, libdir);
    global_alloc.deallocate(libdir.data);
#endif

    global_alloc.deallocate(cwd.data);
  }

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
  while (!false) {
    core::get_named_arena(core::ArenaName::Frame).reset();
    utils::timings_frame_start();
    defer { utils::timings_frame_end(); };

    /// === Frame Callbacks ===
    auto fs_process_scope = utils::scope_start("fs process"_hs);
    fs::process_modified_file_callbacks();
    utils::scope_end(fs_process_scope);

    /// === Frame Udpate ===
    auto sev = app_pfns.frame(*app);

    /// === Handle system event ===

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
  LOG_INFO("Bye");
}
