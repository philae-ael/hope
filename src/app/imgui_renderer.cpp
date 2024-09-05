#include "imgui_renderer.h"

#include <core/vulkan/subsystem.h>
#include <core/vulkan/sync.h>

ImGuiRenderer ImGuiRenderer::init(subsystem::video& v) {
  ImGuiRenderer imgui_renderer;
  VkFormat format;
  switch (v.swapchain.config.surface_format.format) {
  case VK_FORMAT_B8G8R8A8_SRGB:
  case VK_FORMAT_B8G8R8A8_UNORM:
    format = VK_FORMAT_B8G8R8A8_UNORM;
    break;
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_R8G8B8A8_UNORM:
    format = VK_FORMAT_R8G8B8A8_UNORM;
  default:
    break;
  }

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

  imgui_renderer.image = vk::image2D::create(
      v,
      {
          .format = format,
          .extent = {.swapchain{}},
          .usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      },
      {}
  );

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
      .MinAllocationSize = 1024 * 1024,
  };
  ImGui_ImplVulkan_Init(&imgui_implvulkan_info);

  return imgui_renderer;
}
void ImGuiRenderer::uninit(subsystem::video& v) {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();

  image.destroy(v);
  vkDestroyDescriptorPool(v.device, descriptor_pool, nullptr);
  descriptor_pool = VK_NULL_HANDLE;
}

void ImGuiRenderer::render(VkCommandBuffer cmd) {
  vk::pipeline_barrier(cmd, image.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}));
  {
    VkRenderingAttachmentInfo color_attachment{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = image.image_view,
        .imageLayout = image.sync.layout,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = {.color = {.float32 = {1.0, 1.0, 1.0, 1.0}}}
    };
    VkRenderingInfo rendering_info{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{}, image.extent2},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment

    };
    vkCmdBeginRendering(cmd, &rendering_info);
  }

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}
