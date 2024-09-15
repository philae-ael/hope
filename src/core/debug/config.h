#ifndef INCLUDE_DEBUG_CONFIG_H_
#define INCLUDE_DEBUG_CONFIG_H_

namespace debug {
// As soon as possible after imgui::NewFrame
void config_new_frame();
void config_bool(const char* label, bool* target);
} // namespace debug

#endif // INCLUDE_DEBUG_CONFIG_H_
