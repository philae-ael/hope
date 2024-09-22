#include "imgui_renderer.h"

#include <core/utils/time.h>
#include <core/vulkan/subsystem.h>
#include <core/vulkan/sync.h>

ImGuiRenderer ImGuiRenderer::init(subsystem::video& v) {
  ImGuiRenderer imgui_renderer;

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
  VK_ASSERT(
      vkCreateDescriptorPool(v.device, &imgui_descriptor_pool_create_info, nullptr, &imgui_renderer.descriptor_pool)
  );

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
              .pColorAttachmentFormats = &v.swapchain.config.surface_format.format,
          },
      .MinAllocationSize = 1024 * 1024,
  };
  ImGui_ImplVulkan_Init(&imgui_implvulkan_info);

  return imgui_renderer;
}
void ImGuiRenderer::uninit(subsystem::video& v) {
  ImGui_ImplVulkan_Shutdown();

  vkDestroyDescriptorPool(v.device, descriptor_pool, nullptr);
  descriptor_pool = VK_NULL_HANDLE;
}

void ImGuiRenderer::render(VkCommandBuffer cmd, vk::image2D& image) {
  using namespace core::literals;
  auto imgui_scope = utils::scope_start("imgui"_hs);
  defer { utils::scope_end(imgui_scope); };

  vk::pipeline_barrier(cmd, image.sync_to({VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}));
  {
    core::array color_attachments{
        image.as_attachment(vk::image2D::AttachmentLoadOp::Load, vk::image2D::AttachmentStoreOp::Store),
    };
    VkRenderingInfo rendering_info{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{}, image.extent2},
        .layerCount           = 1,
        .colorAttachmentCount = (u32)color_attachments.size(),
        .pColorAttachments    = color_attachments.data
    };
    vkCmdBeginRendering(cmd, &rendering_info);
  }

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}
