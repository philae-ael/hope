#ifndef INCLUDE_DEBUG_CONFIG_H_
#define INCLUDE_DEBUG_CONFIG_H_

#include "core/core/fwd.h"
namespace debug {
// As soon as possible after imgui::NewFrame
void config_new_frame();
void config_bool(const char* label, bool* target);
void config_u64(const char* label, u64* target);
void config_f32(const char* label, f32* target);
} // namespace debug

#endif // INCLUDE_DEBUG_CONFIG_H_
