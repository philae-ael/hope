
#include "renderer.h"
#include "imgui_renderer.h"
#include "triangle_renderer.h"

#include <core/core.h>
#include <core/fs/fs.h>
#include <core/utils/config.h>
#include <core/utils/time.h>
#include <core/vulkan.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/image.h>
#include <core/vulkan/init.h>
#include <core/vulkan/subsystem.h>
#include <core/vulkan/sync.h>
#include <core/vulkan/timings.h>
#include <loader/app_loader.h>
#include <vulkan/vulkan_core.h>

#include <imgui.h>

using namespace core::enum_helpers;
using namespace core::literals;

MainRenderer MainRenderer::init(subsystem::video& v) {
  MainRenderer self;
  self.triangle_renderer = TriangleRenderer::init(v, v.swapchain.config.surface_format.format);
  self.imgui_renderer    = ImGuiRenderer::init(v);
  return self;
}

void MainRenderer::render(AppState* app_state, VkCommandBuffer cmd, vk::image2D& swapchain_image) {
  vk::pipeline_barrier(cmd, swapchain_image.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}));

  auto triangle_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "triangle"_hs);
  triangle_renderer.render(app_state, cmd, swapchain_image);
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, triangle_scope);

  auto imgui_scope = vk::timestamp_scope_start(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "imgui"_hs);
  imgui_renderer.render(cmd, swapchain_image);
  vk::timestamp_scope_end(cmd, VK_PIPELINE_STAGE_2_NONE, imgui_scope);

  vk::pipeline_barrier(
      cmd, swapchain_image.sync_to({
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
               VK_ACCESS_2_NONE,
           })
  );
}

void MainRenderer::uninit(subsystem::video& v) {
  triangle_renderer.uninit(v);
  imgui_renderer.uninit(v);
}

core::vec<core::str8> MainRenderer::file_deps(core::Arena& arena) {
  return core::vec{triangle_renderer.file_deps()}.clone(arena);
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
    scratch.retire();
  }

  return &rdata;
}

AppEvent render(AppState* app_state, subsystem::video& v, Renderer& renderer) {
  AppEvent sev{};
  if (renderer.need_rebuild) {
    sev |= AppEvent::RebuildRenderer;
  }

  auto frame = v.begin_frame();
  if (frame.is_err()) {
    switch (frame.err()) {
    case VK_TIMEOUT:
      sev |= AppEvent::SkipRender;
      return sev;
    case VK_ERROR_OUT_OF_DATE_KHR:
      sev |= AppEvent::RebuildSwapchain;
      return sev;
    case VK_SUBOPTIMAL_KHR:
      sev |= AppEvent::RebuildSwapchain;
      break;
    default:
      VK_ASSERT(frame.err());
    }
  }

  VkCommandBufferBeginInfo command_buffer_begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(renderer.cmd, &command_buffer_begin_info);
  renderer.main_renderer.render(app_state, renderer.cmd, frame->swapchain_image);

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
  renderer.main_renderer.uninit(v);
  renderer.main_renderer = MainRenderer::init(v);
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
