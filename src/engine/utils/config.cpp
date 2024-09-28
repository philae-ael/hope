#include <core/containers/vec.h>
#include <core/core.h>
#include <imgui.h>

union entry {
  enum class Tag { boolean, u64, f32, f32xN };
  struct {
    Tag tag;
    const char* label;
    bool read_only;
  } base;
  struct {
    Tag tag = Tag::boolean;
    const char* label;
    bool read_only;
    bool* value;
  } boolean;
  struct {
    Tag tag = Tag::u64;
    const char* label;
    bool read_only;
    u64* value;
  } u64;
  struct {
    Tag tag = Tag::f32;
    const char* label;
    bool read_only;
    ::f32* value;
  } f32;
  struct {
    Tag tag = Tag::f32xN;
    const char* label;
    bool read_only;
    ::f32* value;
    usize components;
  } f32xN;
};
entry entries[150]{};
static core::vec<entry> v{core::clear, entries};

namespace utils {
EXPORT void config_reset() {
  v.reset(core::noalloc);
}
EXPORT void config_new_frame() {
  if (ImGui::Begin("config")) {
    static ImGuiTextFilter filter;
    filter.Draw();

    for (auto& entry : v.iter()) {
      if (filter.PassFilter(entry.base.label)) {
        if (entry.base.read_only) {
          ImGui::BeginDisabled();
        }
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
        case entry::Tag::f32xN:
          ImGui::InputScalarN(entry.f32xN.label, ImGuiDataType_Float, entry.f32xN.value, (int)entry.f32xN.components);
          break;
        }
        if (entry.base.read_only) {
          ImGui::EndDisabled();
        }
      }
    }
  }
  ImGui::End();
  v.reset(core::noalloc);
}

EXPORT void config_bool(const char* label, bool* target, bool read_only) {
  v.push(
      core::noalloc,
      {
          .boolean{
              .label     = label,
              .read_only = read_only,
              .value     = target,
          },
      }
  );
}
EXPORT void config_u64(const char* label, u64* target, bool read_only) {
  v.push(
      core::noalloc,
      {
          .u64{
              .label     = label,
              .read_only = read_only,
              .value     = target,
          },
      }
  );
}
EXPORT void config_f32(const char* label, f32* target, bool read_only) {
  v.push(
      core::noalloc,
      {
          .f32{
              .label     = label,
              .read_only = read_only,
              .value     = target,
          },
      }
  );
}
EXPORT void config_f32xN(const char* label, f32* target, usize components, bool read_only) {
  v.push(
      core::noalloc,
      {
          .f32xN{
              .label      = label,
              .read_only  = read_only,
              .value      = target,
              .components = components,
          },
      }
  );
}
} // namespace utils
