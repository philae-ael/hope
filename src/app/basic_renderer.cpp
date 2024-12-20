#include "basic_renderer.h"
#include "app/renderer.h"
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
#include <engine/utils/config.h>
#include <engine/utils/time.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

using math::Mat4;

core::str8 shader_vert_src = "/assets/shaders/tri.vert.spv"_s;
core::str8 shader_frag_src = "/assets/shaders/tri.frag.spv"_s;

BasicRenderer BasicRenderer::init(
    subsystem::video& v,
    VkFormat color_format,
    VkFormat depth_format,
    VkDescriptorSetLayout camera_descriptor_layout,
    VkDescriptorSetLayout gpu_texture_descriptor_layout
) {
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;

  auto code_vert = fs::read_all(alloc, shader_vert_src);
  auto code_frag = fs::read_all(alloc, shader_frag_src);
  ASSERT(code_vert.size % sizeof(u32) == 0);
  ASSERT(code_frag.size % sizeof(u32) == 0);

  VkShaderModule module_vert = vk::pipeline::ShaderBuilder{code_vert}.build(v.device);
  VkShaderModule module_frag = vk::pipeline::ShaderBuilder{code_frag}.build(v.device);
  defer {
    vkDestroyShaderModule(v.device, module_vert, nullptr);
    vkDestroyShaderModule(v.device, module_frag, nullptr);
  };

  VkPipelineLayout layout = vk::pipeline::PipelineLayoutBuilder{
      .set_layouts          = {camera_descriptor_layout, gpu_texture_descriptor_layout},
      .push_constant_ranges = {vk::pipeline::PushConstantRanges<PushConstant>{}.vk()}
  }.build(v.device);

  VkPipeline pipeline = vk::pipeline::PipelineBuilder{
      .rendering = {.color_attachment_formats = {1, &color_format}, .depth_attachment_format = depth_format},
      .shader_stages =
          {vk::pipeline::ShaderStage{VK_SHADER_STAGE_VERTEX_BIT, module_vert, "main"}.vk(),
           vk::pipeline::ShaderStage{VK_SHADER_STAGE_FRAGMENT_BIT, module_frag, "main"}.vk()},
      .vertex_input =
          {
              {vk::pipeline::VertexBinding<Vertex>{0}.vk()},
              vk::pipeline::VertexInputAttributes<Vertex>{0}.vk(),
          },
      .depth_stencil = vk::pipeline::DepthStencil::WriteAndCompareDepth,
      .color_blend   = {vk::pipeline::ColorBlendAttachement::NoBlend.vk()},
      .dynamic_state = {{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
  }.build(v.device, layout);

  return {pipeline, layout};
}

void BasicRenderer::render(
    VkDevice device,
    VkCommandBuffer cmd,
    VkDescriptorSet camera_descriptor_set,
    VkDescriptorSet gpu_texture_descriptor_set,
    core::storage<GpuMesh> meshes
) {
  auto triangle_scope = utils::scope_start("triangle"_hs);
  defer { utils::scope_end(triangle_scope); };
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  static bool enable = true;
  utils::config_bool("mesh.enable", &enable);
  if (!enable)
    return;

  core::array descriptor_sets{
      camera_descriptor_set,
      gpu_texture_descriptor_set,
  };
  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (u32)descriptor_sets.size(), descriptor_sets.data, 0, nullptr
  );

  // Do render

  for (auto& mesh : meshes.iter()) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertex_buffer, &offset);

    vk::PushConstantUploader<PushConstant>{mesh.transform, 1 + u32(mesh.base_color_texture_idx)}.upload(cmd, layout);

    vkCmdBindIndexBuffer(cmd, mesh.index_buffer, 0, mesh.huge_indices ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, mesh.indice_count, 1, 0, 0, 0);
  }
}

BindlessTextureDescriptor BindlessTextureDescriptor::init(subsystem::video& v) {
  VkDescriptorPool pool = vk::DescriptorPoolBuilder{
      .max_sets = 1, .sizes = {VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2000}}
  }.build(v.device);

  VkDescriptorSetLayout layout = vk::DescriptorSetLayoutBuilder{
      .bindings = {VkDescriptorSetLayoutBinding{
          0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2000, VK_SHADER_STAGE_FRAGMENT_BIT
      }}
  }.build(v.device);

  VkDescriptorSet set = vk::DescriptorSetAllocateInfo{pool, {layout}}.allocate(v.device);

  auto default_texture = v.create_image2D(
      vk::image2D::Config{
          .format = VK_FORMAT_R8G8B8A8_UNORM,
          .extent = {.constant = {.width = 1, .height = 1}},
          .usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      },
      {}
  );

  return {
      pool,
      layout,
      set,
      default_texture,
  };
}

void BindlessTextureDescriptor::update(VkDevice device, VkSampler sampler, const TextureCache& texture_cache) {
  auto scratch          = core::scratch_get();
  core::Allocator alloc = scratch;
  core::vec<VkDescriptorImageInfo> image_infos;
  image_infos.set_capacity(alloc, 1 + texture_cache.textures.size());
  image_infos.push(
      alloc,
      VkDescriptorImageInfo{
          .sampler     = sampler,
          .imageView   = default_texture.image_view,
          .imageLayout = default_texture.sync.layout,
      }
  );
  for (auto& tex : texture_cache.textures) {
    image_infos.push(
        alloc,
        VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = tex.image_view,
            .imageLayout = tex.sync.layout,
        }
    );
  }
  vk::DescriptorSetUpdater{
      .write_descriptor_set{
          vk::DescriptorSetWriter{
              .set               = set,
              .dst_binding       = 0,
              .dst_array_element = 0,
              .descriptor_type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .image             = {.image_infos = image_infos},
          }
              .vk(),
      },
  }
      .update(device);
}

void BindlessTextureDescriptor::uninit(subsystem::video& v) {
  vkDestroyDescriptorSetLayout(v.device, layout, nullptr);
  vkDestroyDescriptorPool(v.device, pool, nullptr);
  default_texture.destroy(v.device);
}

CameraDescriptor CameraDescriptor::init(subsystem::video& v) {
  VkDescriptorPool pool = vk::DescriptorPoolBuilder{
      .max_sets = 1, .sizes = {VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1}}
  }.build(v.device);

  VkDescriptorSetLayout layout = vk::DescriptorSetLayoutBuilder{
      .bindings = {VkDescriptorSetLayoutBinding{
          0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
      }}
  }.build(v.device);

  VkDescriptorSet set = vk::DescriptorSetAllocateInfo{pool, {layout}}.allocate(v.device);

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
      vk::DescriptorSetWriter{
          .set               = set,
          .dst_binding       = 0,
          .dst_array_element = 0,
          .descriptor_type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .buffer =
              {
                  .buffer_infos = VkDescriptorBufferInfo{buffer, 0, sizeof(CameraMatrices)},
              }
      }.vk(),
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
