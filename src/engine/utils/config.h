#ifndef INCLUDE_DEBUG_CONFIG_H_
#define INCLUDE_DEBUG_CONFIG_H_

#include <core/core/fwd.h>

namespace utils {
// As soon as possible after imgui::NewFrame
void config_new_frame();
void config_reset();
void config_bool(const char* label, bool* target, bool read_only = false);
void config_u64(const char* label, u64* target, bool read_only = false);
void config_f32(const char* label, f32* target, bool read_only = false);
void config_f32xN(const char* label, f32* target, usize components, bool read_only = false);
void config_choice(
    const char* label,
    int* target,
    const char* const* choices,
    usize choice_count,
    bool read_only = false
);
} // namespace utils

#endif // INCLUDE_DEBUG_CONFIG_H_
