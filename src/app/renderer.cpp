
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
using namespace core::literals;

static core::array deps{
    "/assets/scenes/sponza.glb"_s,
};

MainRenderer MainRenderer::init(subsystem::video& v) {
  MainRenderer self;

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
  vkCreateSampler(v.device, &sampler_create_info, nullptr, &self.default_sampler);
  self.swapchain_rebuilt(v);

  self.camera_descriptor      = CameraDescriptor::init(v);
  self.gpu_texture_descriptor = GpuTextureDescriptor::init(v);
  self.basic_renderer         = BasicRenderer::init(
      v, v.swapchain.config.surface_format.format, self.depth.format, self.camera_descriptor.layout,
      self.gpu_texture_descriptor.layout
  );
  self.grid_renderer =
      GridRenderer::init(v, v.swapchain.config.surface_format.format, self.depth.format, self.camera_descriptor.layout);
  self.debug_renderer = DebugRenderer::init(
      v, v.swapchain.config.surface_format.format, self.depth.format, self.camera_descriptor.layout
  );
  self.imgui_renderer = ImGuiRenderer::init(v);

  self.mesh_loader      = MeshLoader::init(v.device);
  self.first_cmd_buffer = true;

  return self;
}

void MainRenderer::render(AppState* app_state, vk::Device& device, VkCommandBuffer cmd, vk::image2D& swapchain_image) {
  if (first_cmd_buffer) {
    // TODO: send this in a second thread and voilà
    mesh_loader.queue_mesh(device, cmd, deps[0]);
    first_cmd_buffer = false;
  }

  vk::pipeline_barrier(
      cmd, swapchain_image.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}),
      depth.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}, VK_IMAGE_ASPECT_DEPTH_BIT)
  );

  mesh_loader.work(
      device,
      [](void* u, vk::Device& device, MeshToken, GpuMesh mesh, bool) {
        auto* self = static_cast<MainRenderer*>(u);
        self->meshes.push(core::get_named_allocator(core::AllocatorName::General), mesh);
        self->should_update_texture_descriptor = true;
      },
      this
  );

  if (should_update_texture_descriptor) {
    for (auto& mesh : meshes.iter()) {
      vk::pipeline_barrier(cmd, mesh.base_color.sync_to({VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}));
    }
    gpu_texture_descriptor.update(device, default_sampler, meshes);
    should_update_texture_descriptor = false;
  }
  auto camera_matrices =
      app_state->camera.matrices((f32)swapchain_image.extent.width, (f32)swapchain_image.extent.height);
  camera_descriptor.update(device, camera_matrices);

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
  basic_renderer.render(device, cmd, camera_descriptor, gpu_texture_descriptor, meshes);
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, mesh_scope);

  auto grid_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "grid"_hs);
  grid_renderer.render(app_state->config.grid, device, cmd, camera_descriptor);
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, grid_scope);

  debug_renderer.reset();
  debug_renderer.draw_origin_gizmo(device);

  auto debug_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "debug"_hs);
  debug_renderer.render(device, cmd, camera_descriptor);
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
  for (auto& mesh : meshes.iter())
    unload_mesh(v, mesh);
  meshes.deallocate(core::get_named_allocator(core::AllocatorName::General));

  basic_renderer.uninit(v.device);
  grid_renderer.uninit(v.device);
  debug_renderer.uninit(v.device);
  imgui_renderer.uninit(v);
  camera_descriptor.uninit(v);
  gpu_texture_descriptor.uninit(v);
  depth.destroy(v.device);
  vkDestroySampler(v.device, default_sampler, nullptr);

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
      "/assets/shaders/tri.spv"_s,
      "/assets/shaders/grid.spv"_s,
      "/assets/shaders/debug.spv"_s,
  }
      .clone(arena);
}

void on_dep_file_modified(void* userdata) {
  LOG_INFO("renderer's file_dep detected, rebuilding renderer is needed");
  auto& renderer        = *static_cast<Renderer*>(userdata);
  renderer.need_rebuild = true;
}

Renderer* init_renderer(core::Allocator alloc, subsystem::video& v) {
  Renderer& rdata    = *new (alloc.allocate<Renderer>()) Renderer{};
  rdata.need_rebuild = false;

  VkCommandPoolCreateInfo command_pool_create_info{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = v.device.omni_queue_family_index,
  };
  VK_ASSERT(vkCreateCommandPool(v.device, &command_pool_create_info, nullptr, &rdata.command_pool));
  VkCommandBufferAllocateInfo command_pool_allocate_info{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = rdata.command_pool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VK_ASSERT(vkAllocateCommandBuffers(v.device, &command_pool_allocate_info, &rdata.cmd));
  rdata.main_renderer = MainRenderer::init(v);

  {
    auto scratch = core::scratch_get();
    auto deps    = rdata.main_renderer.file_deps(*scratch);
    for (auto f : deps.iter()) {
      auto handle = fs::register_modified_file_callback(f, on_dep_file_modified, &rdata);
      rdata.on_file_modified_handles.push(alloc, handle);
    }
  }

  return &rdata;
}

AppEvent render(AppState* app_state, subsystem::video& v, Renderer& renderer) {
  AppEvent sev{};
  if (renderer.need_rebuild) {
    sev |= AppEvent::RebuildRenderer;
  }

  auto t                        = v.begin_frame();
  auto frame                    = get<0>(t);
  auto should_rebuild_swapchain = get<1>(t);
  if (should_rebuild_swapchain) {
    sev |= AppEvent::RebuildSwapchain;
  }
  if (frame.is_none()) {
    sev |= AppEvent::SkipRender;
    return sev;
  }

  VkCommandBufferBeginInfo command_buffer_begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(renderer.cmd, &command_buffer_begin_info);
  renderer.main_renderer.render(app_state, v.device, renderer.cmd, frame->swapchain_image);

  using namespace core::literals;
  auto render_scope = utils::scope_start("end cmd buffer"_hs);
  vkEndCommandBuffer(renderer.cmd);
  utils::scope_end(render_scope);

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

void swapchain_rebuilt(subsystem::video& v, Renderer& renderer) {
  LOG_TRACE("swapchain rebuilt");
  vkDeviceWaitIdle(v.device);
  renderer.main_renderer.swapchain_rebuilt(v);
}

void uninit_renderer(subsystem::video& v, Renderer& renderer) {
  vkDeviceWaitIdle(v.device);
  renderer.main_renderer.uninit(v);

  vkFreeCommandBuffers(v.device, renderer.command_pool, 1, &renderer.cmd);
  vkDestroyCommandPool(v.device, renderer.command_pool, nullptr);

  for (auto h : renderer.on_file_modified_handles.iter()) {
    fs::unregister_modified_file_callback(h);
  }
}
