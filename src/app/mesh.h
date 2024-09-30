#ifndef INCLUDE_APP_MESH_LOADER_H_
#define INCLUDE_APP_MESH_LOADER_H_

#include <core/containers/handle_map.h>
#include <core/containers/vec.h>
#include <core/math/math.h>
#include <engine/graphics/subsystem.h>
#include <engine/graphics/vulkan.h>

#include <vk_mem_alloc.h>

struct GpuMesh {
  VkBuffer vertex_buffer;
  VkBuffer index_buffer;
  VmaAllocation vertex_buf_allocation;
  VmaAllocation index_buf_allocation;
  vk::image2D base_color;

  math::Mat4 transform = math::Mat4::Id;
  u32 indice_count;
};

core::vec<GpuMesh> load_mesh(core::Allocator alloc, subsystem::video&, core::str8 src);
void unload_mesh(subsystem::video&, GpuMesh);

struct _StagingBufferToken {};
using StagingBufferToken = core::handle_t<_StagingBufferToken, u32>;

struct _MeshToken {};
using MeshToken = core::handle_t<_MeshToken, u32>;

struct Job {
  MeshToken mesh_token;
  StagingBufferToken staging_token;
  GpuMesh mesh;
};

// TODO: this is a coroutine... I should dive into c++ coroutines... I guess... maybe...
class MeshLoader {
  using Callback = void (*)(void*, MeshToken, GpuMesh, bool mesh_fully_loaded);
  MeshToken queue_mesh(subsystem::video& v, core::str8 src);
  void work(Callback callback, void* userdata) {
    // Do work
    // Get staging token
    for (auto [tok, buffer] : staging_buffers.iter_enumerate()) {
      usize idx = jobs.size();
      for (auto& job : jobs.iter_rev()) {
        idx -= 1;
        if (job.staging_token == tok) {
          bool mesh_fully_loaded  = false;
          auto& counter           = mesh_job_counter[job.mesh_token].expect("counter does not exist!");
          counter                -= 1;
          if (counter == 0) {
            mesh_fully_loaded = true;
            mesh_job_counter.destroy(job.mesh_token);
          }

          callback(userdata, job.mesh_token, job.mesh, mesh_fully_loaded);
          jobs.swap_last_pop(idx);
        }
      }
    }
  }

  bool loading() {
    return jobs.size() > 0;
  }

private:
  core::vec<Job> jobs;
  core::handle_map<usize, MeshToken> mesh_job_counter;
  core::handle_map<VkBuffer, StagingBufferToken> staging_buffers;
};

#endif // INCLUDE_APP_MESH_LOADER_H_
