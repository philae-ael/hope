
#include "renderer.h"
#include "app/app.h"
#include "app/debug_renderer.h"
#include "app/mesh.h"
#include "basic_renderer.h"
#include "core/core/memory.h"
#include "engine/graphics/vulkan/rendering.h"
#include "imgui_renderer.h"

#include <core/core.h>
#include <core/fs/fs.h>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan.h>
#include <engine/graphics/vulkan/frame.h>
#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/init.h>
#include <engine/graphics/vulkan/sync.h>
#include <engine/graphics/vulkan/timings.h>
#include <engine/utils/config.h>
#include <engine/utils/time.h>
#include <loader/app_loader.h>
#include <vulkan/vulkan_core.h>

#include <imgui.h>

using namespace core::enum_helpers;

MainRenderer::MainRenderer(GPUDataStorage& gpu_data, subsystem::video& v) {
  swapchain_rebuilt(v);

  basic_renderer = BasicRenderer::init(
      v, v.swapchain.config.surface_format.format, depth.format, gpu_data.camera_descriptor.layout,
      gpu_data.bindless_texture_descriptor.layout
  );
  grid_renderer =
      GridRenderer::init(v, v.swapchain.config.surface_format.format, depth.format, gpu_data.camera_descriptor.layout);
  debug_renderer =
      DebugRenderer::init(v, v.swapchain.config.surface_format.format, depth.format, gpu_data.camera_descriptor.layout);
  imgui_renderer = ImGuiRenderer::init(v);
}

void MainRenderer::render(AppState* app_state, vk::Device& device, VkCommandBuffer cmd, vk::image2D& swapchain_image) {
  vk::pipeline_barrier(
      cmd, swapchain_image.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}),
      depth.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}, VK_IMAGE_ASPECT_DEPTH_BIT)
  );

  auto camera_matrices =
      app_state->camera.matrices((f32)swapchain_image.extent.width, (f32)swapchain_image.extent.height);
  app_state->gpu_data.camera_descriptor.update(device, camera_matrices);

  vk::RenderingInfo{
      .render_area       = {{}, swapchain_image.extent},
      .color_attachments = {swapchain_image.as_attachment(
          vk::image2D::AttachmentLoadOp::Clear{{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}},
          vk::image2D::AttachmentStoreOp::Store
      )},
      .depth_attachment  = depth.as_attachment(
          vk::image2D::AttachmentLoadOp::Clear{{.depthStencil = {1.0, 0}}}, vk::image2D::AttachmentStoreOp::Store
      ),
  }
      .begin_rendering(cmd);

  // Setup dynamic state

  VkRect2D scissor{{}, swapchain_image.extent};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkViewport viewport{
      0, (f32)swapchain_image.extent.height, (f32)swapchain_image.extent.width, -(f32)swapchain_image.extent.height, 0,
      1
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  // render

  auto mesh_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "mesh"_hs);
  basic_renderer.render(
      device, cmd, app_state->gpu_data.camera_descriptor, app_state->gpu_data.bindless_texture_descriptor,
      app_state->gpu_data.meshes
  );
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, mesh_scope);

  auto grid_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "grid"_hs);
  grid_renderer.render(app_state->config.grid, device, cmd, app_state->gpu_data.camera_descriptor);
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, grid_scope);

  debug_renderer.reset();
  debug_renderer.draw_origin_gizmo(device);

  auto debug_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "debug"_hs);
  debug_renderer.render(device, cmd, app_state->gpu_data.camera_descriptor);
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, debug_scope);

  vkCmdEndRendering(cmd);

  vk::pipeline_barrier(cmd, swapchain_image.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}));
  vk::RenderingInfo{
      .render_area       = {{}, swapchain_image.extent},
      .color_attachments = {swapchain_image.as_attachment(
          vk::image2D::AttachmentLoadOp::Load, vk::image2D::AttachmentStoreOp::Store
      )}
  }.begin_rendering(cmd);

  auto imgui_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "imgui"_hs);
  imgui_renderer.render(cmd);
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, imgui_scope);

  vkCmdEndRendering(cmd);

  vk::pipeline_barrier(
      cmd, swapchain_image.sync_to({
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
               VK_ACCESS_2_NONE,
           })
  );
}

void MainRenderer::uninit(subsystem::video& v) {
  basic_renderer.uninit(v.device);
  grid_renderer.uninit(v.device);
  debug_renderer.uninit(v.device);
  imgui_renderer.uninit(v);
  depth.destroy(v.device);

  return;
}
void MainRenderer::swapchain_rebuilt(subsystem::video& v) {
  depth.destroy(v.device);
  depth = v.create_image2D(
      vk::image2D::Config{
          .format            = VK_FORMAT_D16_UNORM,
          .extent            = {.swapchain{}},
          .usage             = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          .image_view_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
      },
      {}
  );
}

core::vec<const core::str8> MainRenderer::file_deps(core::Arena& arena) {
  return core::vec<const core::str8>{
      "/assets/shaders/tri.vert.spv"_s,  "/assets/shaders/tri.frag.spv"_s,   "/assets/shaders/grid.vert.spv"_s,
      "/assets/shaders/grid.frag.spv"_s, "/assets/shaders/debug.vert.spv"_s, "/assets/shaders/debug.frag.spv"_s,
  }
      .clone(arena);
}

AppEvent render(AppState* app_state, subsystem::video& v, Renderer& renderer) {
  AppEvent sev{};
  if (renderer.need_rebuild) {
    sev |= AppEvent::RebuildRenderer;
  }

  auto [frame, should_rebuild_swapchain] = v.begin_frame();
  if (should_rebuild_swapchain) {
    sev |= AppEvent::RebuildSwapchain;
  }
  if (frame.is_none()) {
    sev |= AppEvent::SkipRender;
    return sev;
  }

  renderer.render(app_state, v, frame->swapchain_image);

  switch (VkResult res = v.end_frame(*frame, renderer.cmd); res) {
  case VK_ERROR_OUT_OF_DATE_KHR:
  case VK_SUBOPTIMAL_KHR:
    sev |= AppEvent::RebuildSwapchain;
    return sev;
  default:
    VK_ASSERT(res);
  }

  return sev;
}

void Renderer::swapchain_rebuilt(subsystem::video& v) {
  LOG_TRACE("swapchain rebuilt");

  // Rather than this, i think that a timeline semaphore + a coroutine, well whatever
  // Thgs like
  // on_semaphore_ready([]() {
  //    uninit_old();
  // })

  vkDeviceWaitIdle(v.device);
  main_renderer.swapchain_rebuilt(v);
}

Renderer::Renderer(core::Allocator alloc, GPUDataStorage& gpu_data, subsystem::video& v)
    : main_renderer(gpu_data, v) {
  VkCommandPoolCreateInfo command_pool_create_info{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = v.device.omni_queue_family_index,
  };
  VK_ASSERT(vkCreateCommandPool(v.device, &command_pool_create_info, nullptr, &command_pool));
  VkCommandBufferAllocateInfo command_pool_allocate_info{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = command_pool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VK_ASSERT(vkAllocateCommandBuffers(v.device, &command_pool_allocate_info, &cmd));

  {
    auto scratch = core::scratch_get();
    auto deps    = main_renderer.file_deps(*scratch);
    for (auto f : deps.iter()) {
      auto handle = fs::register_modified_file_callback(
          f,
          [](void* userdata) {
            LOG_INFO("renderer's file_dep detected, rebuilding renderer is needed");
            auto& renderer        = *static_cast<Renderer*>(userdata);
            renderer.need_rebuild = true;
          },
          this
      );

      on_file_modified_handles.push(alloc, handle);
    }
  }
}

void Renderer::uninit(subsystem::video& v) {
  vkDeviceWaitIdle(v.device);
  main_renderer.uninit(v);

  vkFreeCommandBuffers(v.device, command_pool, 1, &cmd);
  vkDestroyCommandPool(v.device, command_pool, nullptr);

  for (auto h : on_file_modified_handles.iter()) {
    fs::unregister_modified_file_callback(h);
  }
}
void Renderer::render(AppState* app_state, subsystem::video& v, vk::image2D& swapchain_image) {

  VkCommandBufferBeginInfo command_buffer_begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &command_buffer_begin_info);

  if (first_cmd_buffer) {
    vk::pipeline_barrier(
        cmd, app_state->gpu_data.bindless_texture_descriptor.default_texture.sync_to({VK_IMAGE_LAYOUT_GENERAL})
    );
    VkImageSubresourceRange range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    VkClearColorValue color{.float32 = {1.0, 0.0, 1.0, 1.0}};
    vkCmdClearColorImage(
        cmd, app_state->gpu_data.bindless_texture_descriptor.default_texture, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range
    );
    first_cmd_buffer = false;
  }

  {
    // === MESH LOADING ===

    struct MeshLoaderWorkEnv {
      GPUDataStorage& gpu_data;
      bool should_update_texture_descriptor = false;
    } env{
        app_state->gpu_data,
    };

    app_state->gpu_data.mesh_loader.work(
        v.device,
        [](void* data, vk::Device& device, MeshToken, GpuMesh mesh, bool) {
          auto& env = *static_cast<MeshLoaderWorkEnv*>(data);
          env.gpu_data.meshes.push(core::get_named_allocator(core::AllocatorName::General), mesh);
          env.should_update_texture_descriptor = true;
        },
        &env
    );

    if (env.should_update_texture_descriptor) {
      app_state->gpu_data.bindless_texture_descriptor.update(
          v.device, app_state->gpu_data.default_sampler, app_state->gpu_data.texture_cache
      );
    }
  }

  main_renderer.render(app_state, v.device, cmd, swapchain_image);

  vkEndCommandBuffer(cmd);
}
