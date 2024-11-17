#include "app.h"
#include "app/mesh.h"
#include "profiler.h"
#include "renderer.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <core/core.h>
#include <core/core/memory.h>
#include <core/os/time.h>
#include <engine/graphics/vulkan/frame.h>
#include <engine/graphics/vulkan/init.h>
#include <engine/utils/config.h>
#include <engine/utils/time.h>
#include <loader/app_loader.h>
#include <type_traits>
#include <vulkan/vulkan_core.h>

using namespace core::enum_helpers;
using math::Quat;
using math::Vec4;

SDL_Gamepad* find_gamepad() {
  int joystick_count;
  SDL_JoystickID* joysticks_ = SDL_GetJoysticks(&joystick_count);
  defer { SDL_free(joysticks_); };

  for (auto joystick : core::storage{usize(joystick_count), joysticks_}.iter()) {
    if (SDL_IsGamepad(joystick)) {
      auto gamepad = SDL_OpenGamepad(joystick);
      LOG_INFO("connected to gamepad %s", SDL_GetGamepadName(gamepad));
      return gamepad;
    }
  }

  LOG_INFO("no gamepad founds");
  return nullptr;
}

AppEvent handle_events(SDL_Event& ev, InputState& input_state) {
  ImGui_ImplSDL3_ProcessEvent(&ev);
  AppEvent sev{};
  switch (ev.type) {
  case SDL_EVENT_QUIT: {
    sev |= AppEvent::Exit;
    break;
  }
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
    sev |= AppEvent::Exit;
    break;
  }
  case SDL_EVENT_KEY_UP: {
    if (ImGui::GetIO().WantCaptureKeyboard) {
      break;
    }
    switch (ev.key.key) {
    case SDLK_Q: {
      sev |= AppEvent::Exit;
      break;
    }
    case SDLK_R: {
      sev |= AppEvent::RebuildRenderer;
      break;
    }
    case SDLK_ESCAPE: {
      sev |= AppEvent::ReloadApp;
      break;
    }
    }
    break;
  }
  case SDL_EVENT_KEY_DOWN: {
    if (ImGui::GetIO().WantCaptureKeyboard) {
      break;
    }
    switch (ev.key.key) {
    case SDLK_R: {
      sev |= AppEvent::RebuildRenderer;
      break;
    }
    case SDLK_ESCAPE: {
      sev |= AppEvent::ReloadApp;
      break;
    }
    }
    break;
  }
  case SDL_EVENT_GAMEPAD_ADDED:
    if (input_state.gamepad == nullptr) {
      input_state.gamepad = SDL_OpenGamepad(ev.gdevice.which);
      LOG_INFO("connected to gamepad %s", SDL_GetGamepadName(input_state.gamepad));
    }
    break;
  case SDL_EVENT_GAMEPAD_REMOVED:
    if (input_state.gamepad && ev.gdevice.which == SDL_GetJoystickID(SDL_GetGamepadJoystick(input_state.gamepad))) {
      LOG_INFO("disconnecting gamepad %s", SDL_GetGamepadName(input_state.gamepad));
      SDL_CloseGamepad(input_state.gamepad);
      input_state.gamepad = nullptr;
      input_state.gamepad = find_gamepad();
    }
    break;
  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    if (input_state.gamepad == nullptr ||
        ev.gbutton.which != SDL_GetJoystickID(SDL_GetGamepadJoystick(input_state.gamepad))) {
      break;
    }
    switch (ev.gaxis.axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
      input_state.x.axisPos = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_LEFTY:
      input_state.z.axisPos = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
      input_state.y.axisNeg = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
      input_state.y.axisPos = f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_RIGHTX:
      input_state.yaw.axisPos = -f32(ev.gaxis.value) / 32767.f;
      break;
    case SDL_GAMEPAD_AXIS_RIGHTY:
      input_state.pitch.axisPos = -f32(ev.gaxis.value) / 32767.f;
      break;
    }
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    if (input_state.gamepad == nullptr ||
        ev.gbutton.which != SDL_GetJoystickID(SDL_GetGamepadJoystick(input_state.gamepad))) {
      break;
    }
    switch (ev.gbutton.button) {
    case SDL_GAMEPAD_BUTTON_START:
      sev |= AppEvent::ReloadApp;
      break;
    }
    break;
  }
  if (ImGui::GetIO().WantCaptureKeyboard) {
    return sev;
  }

  input_state.x.updatePos(ev, SDLK_D, 0, 0);
  input_state.x.updateNeg(ev, SDLK_A, 0, 0);
  input_state.y.updatePos(ev, SDLK_SPACE, 0, 0);
  input_state.y.updateNeg(ev, {}, SDL_KMOD_LSHIFT, SDL_KMOD_LSHIFT);
  input_state.z.updatePos(ev, SDLK_S, 0, 0);
  input_state.z.updateNeg(ev, SDLK_W, 0, 0);
  input_state.pitch.updatePos(ev, SDLK_UP, 0, 0);
  input_state.pitch.updateNeg(ev, SDLK_DOWN, 0, 0);
  input_state.yaw.updatePos(ev, SDLK_LEFT, 0, 0);
  input_state.yaw.updateNeg(ev, SDLK_RIGHT, 0, 0);

  return sev;
}

GPUDataStorage::GPUDataStorage(subsystem::video& v)
    : mesh_loader(v.device) {
  VkSamplerCreateInfo sampler_create_info{
      .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter        = VK_FILTER_LINEAR,
      .minFilter        = VK_FILTER_LINEAR,
      .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias       = 0.0,
      .anisotropyEnable = false,
      .maxAnisotropy    = 1.0,
      .compareEnable    = false,
      .borderColor      = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
  };
  vkCreateSampler(v.device, &sampler_create_info, nullptr, &default_sampler);

  camera_descriptor           = CameraDescriptor::init(v);
  bindless_texture_descriptor = BindlessTextureDescriptor::init(v);
}

void GPUDataStorage::uninit(subsystem::video& v) {
  LOG_TRACE("HERE");
  for (auto& mesh : meshes.iter())
    unload_mesh(v, mesh);
  meshes.reset(core::get_named_allocator(core::AllocatorName::General));
  camera_descriptor.uninit(v);
  bindless_texture_descriptor.uninit(v);
  texture_cache.uninit(v.device);
  mesh_loader.uninit(v);

  vkDestroySampler(v.device, default_sampler, nullptr);
}

extern "C" {

EXPORT App* init(AppState* app_state, subsystem::video* video) {
  static App app;

  if (app_state != nullptr && app_state->version != AppState::CUR_VERSION) {
    LOG_ERROR("Resetting app_state: incompatible state version");
    app_state = nullptr;
  }

  if (app_state == nullptr) {
    auto* mem = core::get_named_allocator(core::AllocatorName::General).allocate<AppState>();
    app_state = new (mem) AppState{
        .gpu_data{*video},
    };

    static core::array deps{
        "/assets/scenes/sponza.glb"_s,
        "/assets/scenes/bistro.glb"_s,
        "/assets/scenes/san miguel.glb"_s,
    };

    app_state->gpu_data.mesh_loader.queue_mesh(video->device, deps[1], app_state->gpu_data.texture_cache);
  }

  app.arena             = &core::arena_alloc();
  core::Allocator alloc = *app.arena;
  app.video             = video;
  app.state             = app_state;
  app.renderer          = new (alloc.allocate<Renderer>()) Renderer{
      alloc,
      app.state->gpu_data,
      *app.video,
  };

  app.input_state.gamepad = find_gamepad();

  return &app;
}
static_assert(std::is_same_v<decltype(&init), PFN_init>);

EXPORT AppState* uninit(App& app, bool keep_app_state) {
  LOG_DEBUG("Uninit app");

  app.renderer->uninit(*app.video);
  core::arena_dealloc(*app.arena);
  if (keep_app_state) {
    return app.state;
  } else {
    app.state->gpu_data.uninit(*app.video);
    return nullptr;
  }
}
static_assert(std::is_same_v<decltype(&uninit), PFN_uninit>);

void update(App& app) {
  utils::config_f32xN("input.speed", app.state->config.speed._coeffs, 2);
  utils::config_f32xN("input.rot_speed", (f32*)&app.state->config.rot_speed, 2);

  auto dt       = utils::get_last_frame_dt().secs();
  auto forward1 = app.state->camera.rotation.rotate(-Vec4::Z);
  auto forward  = (forward1 - forward1.dot(Vec4::Y) * Vec4::Y).normalize_or_zero();
  auto sideway  = app.state->camera.rotation.rotate(Vec4::X);
  auto upward   = Vec4::Y;

  auto horizontal_dir = (app.input_state.x.value() * sideway - app.input_state.z.value() * forward).normalize_or_zero();
  auto sqr            = [](f32 x) -> f32 {
    return x * x;
  };
  auto horizontal_speed_factor = MIN(1.f, sqr(app.input_state.x.value()) + sqr(app.input_state.z.value()));

  static Vec4 input_move  = Vec4::Zero;
  input_move              = Vec4::Zero;
  input_move              = app.state->config.speed.x * horizontal_speed_factor * horizontal_dir;
  input_move             += app.state->config.speed.y * app.input_state.y.value() * upward;
  utils::config_f32xN("input.movement", input_move._coeffs, 3, true);
  //
  app.state->camera.position += dt * input_move;

  app.state->camera.rotation =
      Quat::from_axis_angle(sideway, app.state->config.rot_speed.pitch * dt * app.input_state.pitch.value()) *
      Quat::from_axis_angle(upward, app.state->config.rot_speed.yaw * dt * app.input_state.yaw.value()) *
      app.state->camera.rotation;
}

const char* log_level_choices[] = {
    "trace", "debug", "info", "warn", "error",
};
void debug_stuff(App& app) {
  auto& appconf = app.state->config;

  utils::config_u64("app_state.version", &app.state->version, true);

  utils::config_bool("timing.wait_timing_target", &appconf.wait_timing_target);
  utils::config_u64("timing.timing_target_usec", &appconf.timing_target_usec);
  if (app.state->config.wait_timing_target) {
    auto frame_report_scope = utils::scope_start("wait timing target"_hs);
    defer { utils::scope_end(frame_report_scope); };

    os::sleep(USEC(appconf.timing_target_usec));
  }

  utils::config_bool("debug.frame_report", &appconf.print_frame_report);
  utils::config_bool("debug.frame_report.full", &appconf.print_frame_report_full);
  utils::config_bool("debug.frame_report.gpu_budget", &appconf.print_frame_report_gpu_budget);
  utils::config_bool("debug.frame_report.crash_on_out_of_memory_budget", &appconf.crash_on_out_of_memory_budget);
  if (app.state->config.print_frame_report) {
    auto frame_report_scope = utils::scope_start("frame report"_hs);
    defer { utils::scope_end(frame_report_scope); };

    auto timing_infos = utils::get_last_frame_timing_infos(core::get_named_allocator(core::AllocatorName::Frame));
    if (appconf.print_frame_report_full) {
      for (auto [name, t] : timing_infos.timings.iter()) {
        LOG2_DEBUG(name, " at ", core::format{t, os::TimeFormat::MMM_UUU_NNN});
      }
    }

    LOG2_DEBUG("frame mean: ", core::format{timing_infos.stats.mean_frame_time, os::TimeFormat::MMM_UUU_NNN});
    LOG2_DEBUG("frame low 98: ", core::format{timing_infos.stats.low_95, os::TimeFormat::MMM_UUU_NNN});
    LOG2_DEBUG("frame low 99: ", core::format{timing_infos.stats.low_99, os::TimeFormat::MMM_UUU_NNN});
  }

  if (appconf.print_frame_report_gpu_budget | appconf.crash_on_out_of_memory_budget) {
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(app.video->device.allocator, budgets);

    if (appconf.print_frame_report_gpu_budget) {
      struct formatted {
        float size;
        const char* unit;
      };
      auto format_budget = [](float t) -> formatted {
        ASSERT(t >= 0);

        core::storage<const char*> suffixes{"", "K", "M", "G"};
        for (auto suffix : suffixes.slice(suffixes.size - 1).iter()) {
          if (t < 1000.0f) {
            return {t, suffix};
          }
          t /= 1000.0f;
        }
        return {t, suffixes[suffixes.size - 1]};
      };
      for (auto [i, budget] :
           core::enumerate{core::storage{app.video->device.memory_properties.memoryHeapCount, budgets}.iter()}) {
        auto [usage_size, usage_unit]   = format_budget(f32(budget->usage));
        auto [budget_size, budget_unit] = format_budget(f32(budget->budget));

        const char* src = "CPU";
        if (app.video->device.memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
          src = "GPU";
        }

        LOG_DEBUG("budget %s (%zu): %.2f%s/%.2f%s", src, i, usage_size, usage_unit, budget_size, budget_unit);
      }
    }

    if (appconf.crash_on_out_of_memory_budget) {
      for (auto budget : core::storage{app.video->device.memory_properties.memoryHeapCount, budgets}.iter()) {
        ASSERT(budget.budget > budget.usage);
      }
    }
  }

  utils::config_f32xN("camera.position", app.state->camera.position._coeffs, 3);
  utils::config_f32xN("camera.rotation", app.state->camera.rotation.v._coeffs, 4);
  utils::config_f32("camera.fov", &app.state->camera.hfov);
  utils::config_f32("camera.near_plane", &app.state->camera.near);
  utils::config_f32("camera.far_plane", &app.state->camera.far);

  static int log_level = (int)core::log_get_global_level();
  utils::config_choice("debug.log_level", &log_level, log_level_choices, ARRAY_SIZE(log_level_choices));
  if (log_level != (int)core::log_get_global_level()) {
    core::LogLevel log_level_from_choice[]{
        core::LogLevel::Trace,   core::LogLevel::Debug, core::LogLevel::Info,
        core::LogLevel::Warning, core::LogLevel::Error,
    };
    LOG_INFO("setting log level to %s", log_level_choices[log_level]);
    core::log_set_global_level(log_level_from_choice[log_level]);
  }
}

EXPORT AppEvent process_events(App& app) {
  auto poll_event_scope = utils::scope_start("process_events start"_hs);
  defer { utils::scope_end(poll_event_scope); };

  AppEvent sev{};

  SDL_Event ev{};
  while (SDL_PollEvent(&ev)) {
    sev |= handle_events(ev, app.input_state);
  }

  if (app.video->wait_frame(0)) {
    // a frame is ready to be rendered to
    sev |= AppEvent::NewFrame;
  }

  return sev;
}

EXPORT AppEvent new_frame(App& app) {
  utils::timings_frame_end();
  utils::timings_frame_start();

  AppEvent sev{};
  core::get_named_arena(core::ArenaName::Frame).reset();

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  utils::config_new_frame();

  // Things that happens once a frame are here

  profiling_window();
  debug_stuff(app);

  update(app);

  sev |= render(app.state, *app.video, *app.renderer);

  if (any(sev & AppEvent::SkipRender)) {
    LOG_TRACE("rendering skiped");
    ImGui::EndFrame();
  }
  if (any(sev & AppEvent::RebuildRenderer)) {
    LOG_INFO("rebuilding renderer");
    app.renderer->uninit(*app.video);
    *app.renderer = Renderer{*app.arena, app.state->gpu_data, *app.video};
  }
  if (any(sev & AppEvent::RebuildSwapchain)) {
    subsystem::video_rebuild_swapchain(*app.video);
    app.renderer->swapchain_rebuilt(*app.video);
  }

  return sev;
}
}
