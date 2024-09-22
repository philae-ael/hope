
#include "app_loader.h"

#include <backends/imgui_impl_sdl3.h>
#include <core/core.h>
#include <core/fs/fs.h>
#include <core/os.h>
#include <core/os/time.h>
#include <core/utils/config.h>
#include <core/utils/time.h>
#include <core/vulkan/subsystem.h>

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
  log_set_global_level(core::LogLevel::Debug);
  utils::timings_init();
  fs::init();

  auto global_alloc = core::get_named_allocator(core::AllocatorName::General);
#ifdef SHARED
  fs::register_path("lib"_s, fs::resolve_path(global_alloc, "build"_s));
#endif

  /// === Load App ===

  AppPFNs app_pfns;
  load_app(app_pfns);

  /// === APP INIT ===

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

    auto sev = app_pfns.frame(*app);

    /// === Handle system event ===

    if (any(sev & AppEvent::Exit)) {
      break;
    }

    auto fs_process_scope = utils::scope_start("fs process"_hs);
    fs::process_modified_file_callbacks();
    utils::scope_end(fs_process_scope);

    if (any(sev & AppEvent::ReloadApp) || need_reload(app_pfns)) {
      LOG_INFO("reloading app");

      auto app_state = app_pfns.uninit(*app);
      reload_app(app_pfns);
      app = app_pfns.init(app_state, &video);
      utils::timings_reset();
      utils::config_reset();
    }
  }

  /// === CLEANUP ===

  LOG_INFO("Exiting...");
  app_pfns.uninit(*app);

  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  subsystem::uninit_video(video);
  SDL_Quit();
  LOG_INFO("Bye");
}
