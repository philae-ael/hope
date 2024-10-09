#include "basic_renderer.h"
#include "app.h"
#include "camera.h"

#include <core/core.h>
#include <core/fs/fs.h>
#include <core/math.h>
#include <cstring>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan/descriptor.h>
#include <engine/graphics/vulkan/image.h>
#include <engine/graphics/vulkan/pipeline.h>
#include <engine/graphics/vulkan/rendering.h>
#include <engine/graphics/vulkan/vulkan.h>
#include <engine/utils/time.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

using namespace core::literals;
using math::Mat4;

core::array deps{
    "/assets/shaders/tri.spv"_s,
};

BasicRenderer BasicRenderer::init(
    subsystem::video& v,
    VkFormat format,
    VkDescriptorSetLayout camera_descriptor_layout,
    VkDescriptorSetLayout gpu_texture_descriptor_layout
) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;

  auto code = fs::read_all(alloc, deps[0]);
  ASSERT(code.size % sizeof(u32) == 0);

  VkShaderModule module = vk::pipeline::ShaderBuilder{code}.build(v.device);

  VkPipelineLayout layout = vk::pipeline::PipelineLayoutBuilder{
      .set_layouts          = {core::array{camera_descriptor_layout, gpu_texture_descriptor_layout}},
      .push_constant_ranges = {vk::pipeline::PushConstantRanges<PushConstant>{}.vk()}
  }.build(v.device);

  VkPipeline pipeline = vk::pipeline::PipelineBuilder{
      .rendering = {.color_attachment_formats = {1, &format}, .depth_attachment_format = VK_FORMAT_D16_UNORM},
      .shader_stages =
          core::array{
              vk::pipeline::ShaderStage{VK_SHADER_STAGE_VERTEX_BIT, module, "main"}.vk(),
              vk::pipeline::ShaderStage{VK_SHADER_STAGE_FRAGMENT_BIT, module, "main"}.vk()
          },
      .vertex_input =
          {
              core::array{vk::pipeline::VertexBinding<Vertex>{0}.vk()},
              vk::pipeline::VertexInputAttributes<Vertex>{0}.vk(),
          },
      .depth_stencil = vk::pipeline::DepthStencil::WriteAndCompareDepth,
      .color_blend   = {core::array{vk::pipeline::ColorBlendAttachement::NoBlend.vk()}},
      .dynamic_state = {core::array{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
  }.build(v.device, layout);

  vkDestroyShaderModule(v.device, module, nullptr);
  return {pipeline, layout};
}

core::storage<core::str8> BasicRenderer::file_deps() {
  return deps;
}

void BasicRenderer::render(
    AppState* app_state,
    VkDevice device,
    VkCommandBuffer cmd,
    vk::image2D color_target,
    vk::image2D depth_target,
    VkDescriptorSet camera_descriptor_set,
    VkDescriptorSet gpu_texture_descriptor_set,
    core::storage<GpuMesh> meshes
) {
  using namespace core::literals;
  auto triangle_scope = utils::scope_start("triangle"_hs);
  defer { utils::scope_end(triangle_scope); };

  vk::RenderingInfo{
      .render_area = {{}, color_target.extent},
      .color_attachments =
          core::array{
              color_target.as_attachment(
                  vk::image2D::AttachmentLoadOp::Clear{{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}},
                  vk::image2D::AttachmentStoreOp::Store
              ),
          },
      .depth_attachment = depth_target.as_attachment(
          vk::image2D::AttachmentLoadOp::Clear{{.depthStencil = {1.0, 0}}}, vk::image2D::AttachmentStoreOp::Store
      ),
  }
      .begin_rendering(cmd);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  core::array descriptor_sets{
      camera_descriptor_set,
      gpu_texture_descriptor_set,
  };
  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (u32)descriptor_sets.size(), descriptor_sets.data, 0, nullptr
  );

  // Setup dynamic state

  VkRect2D scissor{{}, color_target.extent};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkViewport viewport{
      0, (f32)color_target.extent.height, (f32)color_target.extent.width, -(f32)color_target.extent.height, 0, 1
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  // Do render

  for (auto [mesh_idx, mesh] : core::enumerate{meshes.iter()}) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertex_buffer, &offset);

    vk::PushConstantUploader<PushConstant>{
        mesh->transform,
        u32(mesh_idx),
    }
        .upload(cmd, layout);

    vkCmdBindIndexBuffer(cmd, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, mesh->indice_count, 1, 0, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

GpuTextureDescriptor GpuTextureDescriptor::init(subsystem::video& v) {
  VkDescriptorPool pool = vk::DescriptorPoolBuilder{
      .max_sets = 1,
      .sizes    = {core::array{
          VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50},
      }}
  }.build(v.device);

  VkDescriptorSetLayout layout = vk::DescriptorSetLayoutBuilder{
      .bindings = {core::array{
          VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50, VK_SHADER_STAGE_FRAGMENT_BIT},
      }}
  }.build(v.device);

  VkDescriptorSet set = vk::DescriptorSetAllocateInfo{
      pool,
      {core::array{
          layout,
      }}
  }.allocate(v.device);

  return {
      pool,
      layout,
      set,
  };
}

void GpuTextureDescriptor::update(VkDevice device, VkSampler sampler, core::storage<GpuMesh> meshes) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  core::Allocator alloc = scratch;
  core::vec<VkDescriptorImageInfo> image_infos;
  for (auto& mesh : meshes.iter()) {
    image_infos.push(
        alloc,
        VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = mesh.base_color.image_view,
            .imageLayout = mesh.base_color.sync.layout,
        }
    );
  }
  if (image_infos.size() == 0) {
    return;
  }
  vk::DescriptorSetUpdater{
      .write_descriptor_set{
          core::array{
              vk::DescriptorSetWriter{
                  .set               = set,
                  .dst_binding       = 0,
                  .dst_array_element = 0,
                  .descriptor_type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                  .image             = {.image_infos = image_infos},
              }
                  .vk(),
          },
      },
  }
      .update(device);
}

void GpuTextureDescriptor::uninit(subsystem::video& v) {
  vkDestroyDescriptorSetLayout(v.device, layout, nullptr);
  vkDestroyDescriptorPool(v.device, pool, nullptr);
}

CameraDescriptor CameraDescriptor::init(subsystem::video& v) {
  VkDescriptorPool pool = vk::DescriptorPoolBuilder{
      .max_sets = 1,
      .sizes    = {core::array{
          VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
      }}
  }.build(v.device);

  VkDescriptorSetLayout layout = vk::DescriptorSetLayoutBuilder{
      .bindings = {core::array{
          VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
      }}
  }.build(v.device);

  VkDescriptorSet set = vk::DescriptorSetAllocateInfo{
      pool,
      {core::array{
          layout,
      }}
  }.allocate(v.device);

  // **

  VkBufferCreateInfo buffer_create_info{
      .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size                  = sizeof(CameraMatrices),
      .usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &v.device.omni_queue_family_index,
  };
  VmaAllocationCreateInfo allocation_create_info{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
  };
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
  VK_ASSERT(vmaCreateBuffer(
      v.device.allocator, &buffer_create_info, &allocation_create_info, &buffer, &allocation, &allocation_info
  ));

  vk::DescriptorSetUpdater{
      core::array{
          vk::DescriptorSetWriter{
              .set               = set,
              .dst_binding       = 0,
              .dst_array_element = 0,
              .descriptor_type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .buffer =
                  {
                      .buffer_infos = core::array{VkDescriptorBufferInfo{buffer, 0, sizeof(CameraMatrices)}},
                  }
          }.vk(),
      },
  }
      .update(v.device);
  return {pool, layout, set, buffer, allocation, allocation_info};
}

void CameraDescriptor::update(vk::Device& device, CameraMatrices& matrices) {
  memcpy(allocation_info.pMappedData, &matrices, sizeof(CameraMatrices));
}

void CameraDescriptor::uninit(subsystem::video& v) {
  vkDestroyDescriptorSetLayout(v.device, layout, nullptr);
  vkDestroyDescriptorPool(v.device, pool, nullptr);
  vmaDestroyBuffer(v.device.allocator, buffer, allocation);
}
