
#include "imgui_renderer.h"
#include <core/vulkan/sync.h>

#include <core/core.h>
#include <core/vulkan.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/image.h>
#include <core/vulkan/init.h>

#include <core/vulkan/subsystem.h>
#include <imgui.h>
#include <loader/app_loader.h>

using namespace core::enum_helpers;
struct TriangleRenderer {
  VkDescriptorPool descriptor_pool;
  vk::image2D image;

  static TriangleRenderer init(subsystem::video& v);
  void render(VkCommandBuffer cmd);
  void uninit(subsystem::video& v);
};

struct MainRenderer {
  ImGuiRenderer imgui_renderer;
  TriangleRenderer triangle_renderer;

  static MainRenderer init(subsystem::video& v);
  void render(VkCommandBuffer cmd, vk::image2D& swapchain_image);
  void uninit(subsystem::video& v);
};

struct Renderer {
  VkCommandPool command_pool;
  VkCommandBuffer cmd;

  MainRenderer main_renderer;
};

MainRenderer MainRenderer::init(subsystem::video& v) {
  MainRenderer self;
  self.imgui_renderer = ImGuiRenderer::init(v);
  return self;
}

void MainRenderer::render(VkCommandBuffer cmd, vk::image2D& swapchain_image) {
  imgui_renderer.render(cmd);
  vk::pipeline_barrier(
      cmd,
      vk::Barriers{
          swapchain_image.sync_to({VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}),
          imgui_renderer.image.sync_to({VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL})
      }
  );
  {
    VkImageCopy region{
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
        .extent         = imgui_renderer.image.extent3,
    };
    vkCmdCopyImage(
        cmd, imgui_renderer.image, imgui_renderer.image.sync.layout, swapchain_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );
  }
  vk::pipeline_barrier(
      cmd, swapchain_image.sync_to({
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
               VK_ACCESS_2_NONE,
           })
  );
}

void MainRenderer::uninit(subsystem::video& v) {
  imgui_renderer.uninit(v);
}

extern "C" {

EXPORT Renderer* init_renderer(core::Arena& ar, subsystem::video& v) {
  Renderer* rdata = ar.allocate<Renderer>();
  VkCommandPoolCreateInfo command_pool_create_info{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = v.device.omni_queue_family_index,
  };
  VK_ASSERT(vkCreateCommandPool(v.device, &command_pool_create_info, nullptr, &rdata->command_pool)
  );
  VkCommandBufferAllocateInfo command_pool_allocate_info{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = rdata->command_pool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VK_ASSERT(vkAllocateCommandBuffers(v.device, &command_pool_allocate_info, &rdata->cmd));
  rdata->main_renderer = MainRenderer::init(v);

  return rdata;
}

EXPORT AppEvent render(subsystem::video& v, Renderer* renderer) {
  AppEvent sev{};

  auto frame = v.begin_frame();
  if (frame.is_err()) {
    switch (frame.err()) {
    case VK_TIMEOUT:
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

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  defer { ImGui::EndFrame(); };

  static bool show_window = true;
  ImGui::ShowDemoWindow(&show_window);

  {
    VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(renderer->cmd, &command_buffer_begin_info);
  }
  renderer->main_renderer.render(renderer->cmd, frame->swapchain_image);

  vkEndCommandBuffer(renderer->cmd);

  switch (VkResult res = v.end_frame(*frame, renderer->cmd); res) {
  case VK_ERROR_OUT_OF_DATE_KHR:
  case VK_SUBOPTIMAL_KHR:
    sev |= AppEvent::RebuildSwapchain;
    return sev;
  default:
    VK_ASSERT(res);
  }

  return sev;
}

EXPORT void swapchain_rebuilt(subsystem::video& v, Renderer* renderer) {
  LOG_TRACE("swapchain rebuilt");
  vkDeviceWaitIdle(v.device);
  renderer->main_renderer.uninit(v);
  renderer->main_renderer = MainRenderer::init(v);
}

EXPORT void uninit_renderer(subsystem::video& v, Renderer* renderer) {
  vkDeviceWaitIdle(v.device);
  renderer->main_renderer.uninit(v);

  vkFreeCommandBuffers(v.device, renderer->command_pool, 1, &renderer->cmd);
  vkDestroyCommandPool(v.device, renderer->command_pool, nullptr);
}
}
