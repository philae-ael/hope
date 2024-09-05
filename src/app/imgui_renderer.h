#ifndef INCLUDE_APP_IMGUI_H_
#define INCLUDE_APP_IMGUI_H_

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <core/vulkan.h>
#include <core/vulkan/image.h>

struct ImGuiRenderer {
  VkDescriptorPool descriptor_pool;
  vk::image2D image;

  static ImGuiRenderer init(subsystem::video& v);
  void render(VkCommandBuffer);
  void uninit(subsystem::video& v);
};

#endif // INCLUDE_APP_IMGUI_H_
