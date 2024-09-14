
#include "triangle_renderer.h"
#include "core/core.h"
#include "core/debug/time.h"
#include "core/fs/fs.h"
#include "core/vulkan/image.h"
#include "core/vulkan/pipeline.h"
#include <core/vulkan/subsystem.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "cgltf.h"

using namespace core::literals;

core::array deps{
    "assets/shaders/tri.spv"_s,
    "assets/scenes/triangle.glb"_s,
};

TriangleRenderer TriangleRenderer::init(subsystem::video& v, VkFormat format) {
  auto scratch = core::scratch_get();
  defer { scratch.retire(); };

  const char* path      = fs::resolve_path(*scratch, deps[1]).cstring(*scratch);
  cgltf_options options = {
      .type   = cgltf_file_type_glb,
      .memory = {
          .alloc_func = [](void* arena,
                           usize size) { return ((core::Arena*)arena)->allocate(size); },
          .free_func  = [](void* arena, void* ptr) {},
          .user_data  = scratch.arena_,
      },
  };

  cgltf_data* data    = NULL;
  cgltf_result result = cgltf_parse_file(&options, path, &data);
  if (result != cgltf_result_success) {
    core::panic("can't load gltf: %u", result);
  }
  result = cgltf_load_buffers(&options, data, path);
  if (result != cgltf_result_success) {
    core::panic("can't load gltf: %u", result);
  }

  ASSERT(data->meshes_count == 1);
  ASSERT(data->meshes[0].primitives_count == 1);
  ASSERT(data->meshes[0].primitives[0].type == cgltf_primitive_type_triangles);
  // ASSERT(data->meshes[0].primitives[0].attributes_count == 1);
  ASSERT(data->meshes[0].primitives[0].attributes[0].type == cgltf_attribute_type_position);
  ASSERT(data->meshes[0].primitives[0].attributes[0].data->type == cgltf_type_vec3);
  ASSERT(
      data->meshes[0].primitives[0].attributes[0].data->component_type == cgltf_component_type_r_32f
  );
  auto vertices = scratch->allocate_array<f32>(
      data->meshes[0].primitives[0].attributes[0].data->count *
      cgltf_num_components(data->meshes[0].primitives[0].attributes[0].data->type)
  );
  cgltf_accessor_unpack_floats(
      data->meshes[0].primitives[0].attributes[0].data, vertices.data, vertices.size
  );

  VkBuffer vertex_buffer;
  VmaAllocation vertex_buf_alloc;
  VkBufferCreateInfo vertex_buf_create_info{
      .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size                  = vertices.into_bytes().size,
      .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &v.device.omni_queue_family_index
  };
  VmaAllocationCreateInfo alloc_create_info{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
  };
  vmaCreateBuffer(
      v.allocator, &vertex_buf_create_info, &alloc_create_info, &vertex_buffer, &vertex_buf_alloc,
      nullptr
  );
  vmaCopyMemoryToAllocation(
      v.allocator, vertices.data, vertex_buf_alloc, 0, vertices.into_bytes().size
  );

  ASSERT(data->meshes[0].primitives[0].indices->component_type == cgltf_component_type_r_16u);
  auto indices = scratch->allocate_array<u16>(data->meshes[0].primitives[0].indices->count);

  ASSERT(cgltf_accessor_unpack_indices(
      data->meshes[0].primitives[0].indices, indices.data, 2, indices.size
  ));

  VkBuffer index_buffer;
  VmaAllocation index_buf_alloc;
  VkBufferCreateInfo index_buf_create_info{
      .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size                  = vertices.into_bytes().size,
      .usage                 = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices   = &v.device.omni_queue_family_index
  };
  vmaCreateBuffer(
      v.allocator, &index_buf_create_info, &alloc_create_info, &index_buffer, &index_buf_alloc,
      nullptr
  );
  vmaCopyMemoryToAllocation(
      v.allocator, indices.data, index_buf_alloc, 0, indices.into_bytes().size
  );
  Mesh mesh = {(u32)indices.size};
  cgltf_free(data);

  core::array<VkDescriptorPoolSize, 0> pool_sizes{};
  VkDescriptorPoolCreateInfo descriptor_pool_create_info{
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 1,
      .poolSizeCount = (u32)pool_sizes.size(),
      .pPoolSizes    = pool_sizes.data,
  };
  VkDescriptorPool descriptor_pool;
  vkCreateDescriptorPool(v.device, &descriptor_pool_create_info, nullptr, &descriptor_pool);

  // # Layout
  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = 0,
      .pSetLayouts            = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges    = nullptr
  };
  VkPipelineLayout pipeline_layout;
  vkCreatePipelineLayout(v.device, &pipeline_layout_create_info, nullptr, &pipeline_layout);

  auto code = fs::read_all(*scratch, deps[0]);
  ASSERT(code.size % sizeof(u32) == 0);

  // # Pipeline
  VkShaderModuleCreateInfo fragment_module_create_info{
      .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = code.size,
      .pCode    = (u32*)code.data,
  };
  VkShaderModule module;
  VK_ASSERT(vkCreateShaderModule(v.device, &fragment_module_create_info, nullptr, &module));

  core::array shader_stage_create_infos{
      VkPipelineShaderStageCreateInfo{
          .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_VERTEX_BIT,
          .module = module,
          .pName  = "main"
      },
      VkPipelineShaderStageCreateInfo{
          .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = module,
          .pName  = "main"
      },
  };

  core::array vertex_binding_descriptions{VkVertexInputBindingDescription{
      .binding   = 0,
      .stride    = sizeof(f32) * 3,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  }};
  core::array vertex_attribute_descriptions{VkVertexInputAttributeDescription{
      .binding = 0,
      .format  = VK_FORMAT_R32G32B32_SFLOAT,
      .offset  = 0,
  }};

  core::array color_blend_attachments{
      vk::pipeline::ColorBlendAttachement::NoBlend.vk(),
  };
  core::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipeline pipeline =
      vk::pipeline::Pipeline{
          .rendering     = {.color_attachment_formats = {1, &format}},
          .shader_stages = shader_stage_create_infos,
          .vertex_input  = {vertex_binding_descriptions, vertex_attribute_descriptions},
          .color_blend   = {color_blend_attachments},
          .dynamic_state = {dynamic_states},
      }
          .build(v.device, pipeline_layout);

  vkDestroyShaderModule(v.device, module, nullptr);
  return {
      descriptor_pool,  pipeline,     pipeline_layout, vertex_buffer,
      vertex_buf_alloc, index_buffer, index_buf_alloc, mesh,
  };
}

void TriangleRenderer::uninit(subsystem::video& v) {
  vkDestroyPipeline(v.device, pipeline, nullptr);
  vkDestroyPipelineLayout(v.device, pipeline_layout, nullptr);
  vmaDestroyBuffer(v.allocator, vertex_buffer, vertex_buf_allocation);
  vmaDestroyBuffer(v.allocator, index_buffer, index_buf_allocation);
  vkDestroyDescriptorPool(v.device, descriptor_pool, nullptr);
}

core::storage<core::str8> TriangleRenderer::file_deps() {
  return deps;
}

void TriangleRenderer::render(VkCommandBuffer cmd, vk::image2D target) {
  using namespace core::literals;
  auto triangle_scope = debug::scope_start("triangle"_hs);
  defer { debug::scope_end(triangle_scope); };
  core::array color_attachments{target.as_attachment(
      vk::image2D::AttachmentLoadOp::Clear{{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}},
      vk::image2D::AttachmentStoreOp::Store
  )};
  VkRenderingInfo rendering_info{
      .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea           = {{}, target.extent2},
      .layerCount           = 1,
      .colorAttachmentCount = (u32)color_attachments.size(),
      .pColorAttachments    = color_attachments.data
  };

  vkCmdBeginRendering(cmd, &rendering_info);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  VkRect2D scissor{{}, target.extent2};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkViewport viewport{
      0, (f32)target.extent2.height, (f32)target.extent2.width, -(f32)target.extent2.height, 0, 1
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
  vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(cmd, mesh.count, 1, 0, 0, 0);
  vkCmdEndRendering(cmd);
}
