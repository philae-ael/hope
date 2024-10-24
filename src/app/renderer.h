#ifndef INCLUDE_APP_RENDERER_H_
#define INCLUDE_APP_RENDERER_H_

#include "basic_renderer.h"
#include "debug_renderer.h"
#include "grid_renderer.h"
#include "imgui_renderer.h"
#include "loader/app_loader.h"
#include "mesh.h"
#include <engine/graphics/vulkan/image.h>

#include <core/fs/fs.h>

struct MainRenderer {
  CameraDescriptor camera_descriptor;
  GpuTextureDescriptor gpu_texture_descriptor;
  ImGuiRenderer imgui_renderer;
  BasicRenderer basic_renderer;
  GridRenderer grid_renderer;
  DebugRenderer debug_renderer;
  vk::image2D depth;
  VkSampler default_sampler;
  bool should_update_texture_descriptor;
  bool first_cmd_buffer;
  MeshLoader mesh_loader;
  core::vec<GpuMesh> meshes;

  static MainRenderer init(subsystem::video& v);
  void render(AppState* app_state, vk::Device& device, VkCommandBuffer cmd, vk::image2D& swapchain_image);
  void uninit(subsystem::video& v);

  void swapchain_rebuilt(subsystem::video& v);

  core::vec<const core::str8> file_deps(core::Arena& arena);
};

struct Renderer {
  VkCommandPool command_pool;
  VkCommandBuffer cmd;

  core::vec<fs::on_file_modified_handle> on_file_modified_handles;
  bool need_rebuild = false;

  MainRenderer main_renderer;
};

Renderer* init_renderer(core::Allocator alloc, subsystem::video& v);
void uninit_renderer(subsystem::video& v, Renderer& renderer);

AppEvent render(AppState* app_state, subsystem::video& v, Renderer& renderer);
void swapchain_rebuilt(subsystem::video& v, Renderer& renderer);

#endif // INCLUDE_APP_RENDERER_H_
