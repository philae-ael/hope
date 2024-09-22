#include <core/containers/vec.h>
#include <core/core.h>
#include <imgui.h>

union entry {
  enum class Tag { boolean, u64, f32 };
  struct {
    Tag tag;
    const char* label;
  } base;
  struct {
    Tag tag = Tag::boolean;
    const char* label;
    bool* value;
  } boolean;
  struct {
    Tag tag = Tag::u64;
    const char* label;
    u64* value;
  } u64;
  struct {
    Tag tag = Tag::f32;
    const char* label;
    f32* value;
  } f32;
};
static core::Arena* ar;
static core::vec<entry> v;

namespace utils {
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
        case entry::Tag::u64:
          ImGui::InputScalar(entry.u64.label, ImGuiDataType_U64, entry.u64.value);
          break;
        case entry::Tag::f32:
          ImGui::InputScalar(entry.f32.label, ImGuiDataType_Float, entry.f32.value);
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
EXPORT void config_u64(const char* label, u64* target) {
  v.push(
      *ar,
      {
          .u64{
              .label = label,
              .value = target,
          },
      }
  );
}
EXPORT void config_f32(const char* label, f32* target) {
  v.push(
      *ar,
      {
          .f32{
              .label = label,
              .value = target,
          },
      }
  );
}
} // namespace utils
