
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include <core/core.h>
#include <core/vulkan/frame.h>
#include <core/vulkan/init.h>

#include <loader/app_loader.h>
#include <loader/subsystems.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

using namespace core::enum_helpers;

struct ImGuiRenderer {
  VkDescriptorPool descriptor_pool;
  VkImage image;
  VkImageView image_view;
  VkExtent3D image_extent;
  VmaAllocation allocation;

  static ImGuiRenderer init(subsystem::video& v, VmaAllocator allocator);
  void uninit(subsystem::video& v, VmaAllocator allocator);
  void render(subsystem::video& v, VkCommandBuffer cmd);
};

struct Renderer {
  VmaAllocator allocator;
  VkCommandPool command_pool;
  VkCommandBuffer cmd;
  ImGuiRenderer imgui_renderer;
};

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

  VmaAllocatorCreateInfo allocator_create_info = {
      // .flags            = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
      .physicalDevice   = v.device.physical,
      .device           = v.device,
      .instance         = v.instance,
      .vulkanApiVersion = VK_API_VERSION_1_3,
  };
  VK_ASSERT(vmaCreateAllocator(&allocator_create_info, &rdata->allocator));
  rdata->imgui_renderer = ImGuiRenderer::init(v, rdata->allocator);
  return rdata;
}

ImGuiRenderer ImGuiRenderer::init(subsystem::video& v, VmaAllocator allocator) {
  ImGuiRenderer imgui_renderer;
  VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;

  VkDescriptorPoolSize imgui_descriptor_pool_sizes[]{
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
  };
  VkDescriptorPoolCreateInfo imgui_descriptor_pool_create_info{
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets       = 1,
      .poolSizeCount = ARRAY_SIZE(imgui_descriptor_pool_sizes),
      .pPoolSizes    = imgui_descriptor_pool_sizes,
  };
  VK_ASSERT(vkCreateDescriptorPool(
      v.device, &imgui_descriptor_pool_create_info, nullptr, &imgui_renderer.descriptor_pool
  ));

  imgui_renderer.image_extent = {
      v.swapchain.config.extent.width, v.swapchain.config.extent.height, 1
  };
  VkImageCreateInfo imgui_image_create_info{
      .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType   = VK_IMAGE_TYPE_2D,
      .format      = format,
      .extent      = imgui_renderer.image_extent,
      .mipLevels   = 1,
      .arrayLayers = 1,
      .samples     = VK_SAMPLE_COUNT_1_BIT,
      .usage       = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &v.device.omni_queue_family_index,
      .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VmaAllocationCreateInfo alloc_create_info{
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
  };
  VK_ASSERT(vmaCreateImage(
      allocator, &imgui_image_create_info, &alloc_create_info, &imgui_renderer.image,
      &imgui_renderer.allocation, nullptr
  ));

  {
    VkImageViewCreateInfo image_view_create_info{
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image      = imgui_renderer.image,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = format,
        .components = {},
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VK_ASSERT(
        vkCreateImageView(v.device, &image_view_create_info, nullptr, &imgui_renderer.image_view)
    );
  }

  ImGui_ImplSDL3_InitForVulkan(v.window);
  ImGui_ImplVulkan_InitInfo imgui_implvulkan_info{
      .Instance            = v.instance,
      .PhysicalDevice      = v.device.physical,
      .Device              = v.device,
      .QueueFamily         = v.device.omni_queue_family_index,
      .Queue               = v.device.omni_queue,
      .DescriptorPool      = imgui_renderer.descriptor_pool,
      .MinImageCount       = v.swapchain.config.min_image_count,
      .ImageCount          = v.swapchain.config.min_image_count,
      .UseDynamicRendering = true,
      .PipelineRenderingCreateInfo =
          {
              .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
              .colorAttachmentCount    = 1,
              .pColorAttachmentFormats = &format,
          },
  };
  ImGui_ImplVulkan_Init(&imgui_implvulkan_info);

  return imgui_renderer;
}
void ImGuiRenderer::uninit(subsystem::video& v, VmaAllocator allocator) {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();

  vkDestroyImageView(v.device, image_view, nullptr);
  image_view = VK_NULL_HANDLE;
  vmaDestroyImage(allocator, image, allocation);
  image = VK_NULL_HANDLE;
  vkDestroyDescriptorPool(v.device, descriptor_pool, nullptr);
  descriptor_pool = VK_NULL_HANDLE;
}

EXPORT void swapchain_rebuilt(subsystem::video& v, Renderer* renderer) {
  LOG_TRACE("swapchain rebuilt");
  vkDeviceWaitIdle(v.device);
  renderer->imgui_renderer.uninit(v, renderer->allocator);
  renderer->imgui_renderer = ImGuiRenderer::init(v, renderer->allocator);
}

EXPORT void uninit_renderer(subsystem::video& v, Renderer* renderer) {
  auto [allocator, command_pool, cmd, imgui_descriptor_pool] = *renderer;
  vkDeviceWaitIdle(v.device);
  renderer->imgui_renderer.uninit(v, renderer->allocator);

  vmaDestroyAllocator(allocator);
  vkFreeCommandBuffers(v.device, command_pool, 1, &cmd);
  vkDestroyCommandPool(v.device, command_pool, nullptr);
}

void ImGuiRenderer::render(subsystem::video& v, VkCommandBuffer cmd) {
  {
    VkImageMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image         = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkDependencyInfoKHR dep{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dep);
  }
  {
    VkRenderingAttachmentInfo color_attachment{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = image_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = {.color = {.float32 = {0.5, 0.5, 0.5, 1.0}}}
    };
    VkRenderingInfo rendering_info{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{}, v.swapchain.config.extent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment

    };
    vkCmdBeginRendering(cmd, &rendering_info);
  }

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
  {
    VkImageMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image         = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkDependencyInfoKHR dep{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dep);
  }
}

EXPORT AppEvent render(subsystem::video& v, Renderer* renderer) {
  auto cmd = renderer->cmd;
  AppEvent sev{};

  auto frame_ = vk::begin_frame(v.device, v.swapchain, v.sync);
  if (frame_.is_err()) {
    switch (frame_.err()) {
    case VK_TIMEOUT:
      return sev;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
      sev |= AppEvent::RebuildSwapchain;
      return sev;
    default:
      VK_ASSERT(frame_.err());
    }
  }
  auto frame              = frame_.value();
  VkImage swapchain_image = v.swapchain.images[frame.swapchain_image_index];

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  static bool show_window = true;
  ImGui::ShowDemoWindow(&show_window);

  {
    VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &command_buffer_begin_info);
  }
  renderer->imgui_renderer.render(v, cmd);
  {
    VkImageMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image         = swapchain_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkDependencyInfoKHR dep{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dep);
  }
  {
    VkClearColorValue clear_color{.float32 = {1.0, 1.0, 1.0, 0.0}};
    VkImageSubresourceRange clear_range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    vkCmdClearColorImage(
        cmd, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &clear_range
    );
  }
  {

    VkImageCopy region{
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
        .extent         = renderer->imgui_renderer.image_extent,
    };
    vkCmdCopyImage(
        cmd, renderer->imgui_renderer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );
  }
  {
    VkImageMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image         = swapchain_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkDependencyInfoKHR dep{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dep);
  }

  vkEndCommandBuffer(cmd);
  VkPipelineStageFlags acquire_dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkSubmitInfo submit_info{
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = &frame.acquire_semaphore,
      .pWaitDstStageMask    = &acquire_dst_stage,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = &frame.render_semaphore,
  };
  vkQueueSubmit(v.device.omni_queue, 1, &submit_info, frame.render_done_fence);
  VkResult res = vk::end_frame(v.device, v.device.omni_queue, v.swapchain, frame);
  switch (res) {
  case VK_ERROR_OUT_OF_DATE_KHR:
  case VK_SUBOPTIMAL_KHR:
    sev |= AppEvent::RebuildSwapchain;
    break;
  default:
    VK_ASSERT(res);
  }

  ImGui::EndFrame();
  return sev;
}
}
