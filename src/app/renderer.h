#ifndef INCLUDE_APP_RENDERER_H_
#define INCLUDE_APP_RENDERER_H_

#include "imgui_renderer.h"
#include "loader/app_loader.h"
#include "triangle_renderer.h"

#include <core/fs/fs.h>

struct MainRenderer {
  ImGuiRenderer imgui_renderer;
  TriangleRenderer triangle_renderer;

  static MainRenderer init(subsystem::video& v);
  void render(AppState* app_state, VkCommandBuffer cmd, vk::image2D& swapchain_image);
  void uninit(subsystem::video& v);

  core::vec<core::str8> file_deps(core::Arena& arena);
};

struct Renderer {
  VkCommandPool command_pool;
  VkCommandBuffer cmd;

  core::vec<fs::on_file_modified_handle> on_file_modified_handles;
  bool need_rebuild = false;

  MainRenderer main_renderer;
};

Renderer* init_renderer(core::Arena& ar, subsystem::video& v);
void uninit_renderer(subsystem::video& v, Renderer& renderer);

AppEvent render(AppState* app_state, subsystem::video& v, Renderer& renderer);
void swapchain_rebuilt(subsystem::video& v, Renderer& renderer);

#endif // INCLUDE_APP_RENDERER_H_
