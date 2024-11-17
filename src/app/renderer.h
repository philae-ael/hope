#ifndef INCLUDE_APP_RENDERER_H_
#define INCLUDE_APP_RENDERER_H_

#include "basic_renderer.h"
#include "core/core.h"
#include "debug_renderer.h"
#include "grid_renderer.h"
#include "imgui_renderer.h"
#include "mesh.h"

#include <core/fs/fs.h>
#include <engine/graphics/vulkan/image.h>
#include <loader/app_loader.h>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

struct TextureKey {
  core::str8 src;
  usize texture_index;

  auto operator<=>(const TextureKey& other) const = default;
};

template <>
class std::hash<TextureKey> {
public:
  std::size_t operator()(const TextureKey& k) const noexcept {
    core::hasher h{};
    h.hash(k.src.data, k.src.len);
    h.hash(k.texture_index);
    return h.value();
  }
};

struct TextureCache {
  // TODO: Roll my own
  std::vector<vk::image2D> textures{};
  std::unordered_map<TextureKey, usize> textures_idx{};

  union entry_t {
    enum class Tag { Occupied, Empty } tag;
    struct {
      Tag tag = Tag::Occupied;
      usize entry;
    } occupied;
    struct {
      Tag tag = Tag::Empty;
      core::ref_wrapper<TextureCache> self;
      TextureKey key;
    } empty;

    usize or_create(auto f)
      requires std::is_invocable_r_v<vk::image2D, decltype(f)>
    {
      switch (tag) {
      case Tag::Occupied:
        return occupied.entry;
      case Tag::Empty:
        empty.self->textures.push_back(f());
        usize idx = empty.self->textures.size() - 1;
        empty.self->textures_idx.emplace(empty.key, idx);
        return idx;
      }
    }
    bool is_empty() const {
      return tag == Tag::Empty;
    }
  };
  entry_t entry(TextureKey key) {
    auto it = textures_idx.find(key);
    if (it != textures_idx.end()) {
      return entry_t{.occupied = {.entry = it->second}};
    } else {
      return entry_t{.empty = {.self = *this, .key = key}};
    }
  }
  void uninit(vk::Device& device) {
    for (auto& v : textures) {
      v.destroy(device);
    }
    textures_idx.clear();
    textures.clear();
  }

  vk::image2D& at(usize idx) {
    return textures[idx];
  }
};

struct GPUDataStorage;

struct MainRenderer {
  ImGuiRenderer imgui_renderer;
  BasicRenderer basic_renderer;
  GridRenderer grid_renderer;
  DebugRenderer debug_renderer;

  vk::image2D depth;

  MainRenderer(GPUDataStorage& gpu_data, subsystem::video& v);
  void render(AppState* app_state, vk::Device& device, VkCommandBuffer cmd, vk::image2D& swapchain_image);
  void uninit(subsystem::video& v);

  void swapchain_rebuilt(subsystem::video& v);

  core::vec<const core::str8> file_deps(core::Arena& arena);
};

struct Renderer {
  VkCommandPool command_pool;
  VkCommandBuffer cmd;

  bool need_rebuild     = false;
  bool first_cmd_buffer = true;

  MainRenderer main_renderer;

  core::vec<fs::on_file_modified_handle> on_file_modified_handles;

  Renderer(core::Allocator alloc, GPUDataStorage& gpu_data, subsystem::video& v);
  void uninit(subsystem::video& v);

  void swapchain_rebuilt(subsystem::video& v);
  void render(AppState* app_state, subsystem::video& v, vk::image2D& swapchain_image);
};

AppEvent render(AppState* app_state, subsystem::video& v, Renderer& renderer);

#endif // INCLUDE_APP_RENDERER_H_
