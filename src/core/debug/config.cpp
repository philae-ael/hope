#include <core/containers/vec.h>
#include <core/core.h>
#include <imgui.h>

union entry {
  enum class Tag { boolean };
  struct {
    Tag tag;
    const char* label;
  } base;
  struct {
    Tag tag = Tag::boolean;
    const char* label;
    bool* value;
  } boolean;
};
static core::Arena* ar;
static core::vec<entry> v;

namespace debug {
EXPORT void config_new_frame() {
  if (ar == nullptr) {
    ar = &core::arena_alloc();
  }

  if (ImGui::Begin("Config")) {
    static ImGuiTextFilter filter;
    filter.Draw();

    for (auto& entry : v.iter()) {
      if (filter.PassFilter(entry.base.label))
        switch (entry.base.tag) {
        case entry::Tag::boolean:
          ImGui::Checkbox(entry.boolean.label, entry.boolean.value);
          break;
        }
    }
  }
  ImGui::End();

  v.reset(*ar);
}
EXPORT void config_bool(const char* label, bool* target) {
  v.push(
      *ar,
      {
          .boolean{
              .label = label,
              .value = target,
          },
      }
  );
}
} // namespace debug
