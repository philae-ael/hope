add_subdirectory(SDL)
add_subdirectory(VulkanMemoryAllocator)

add_library(imgui STATIC
  imgui/imgui.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/backends/imgui_impl_sdl3.cpp
  imgui/backends/imgui_impl_vulkan.cpp
)
target_compile_options(imgui PRIVATE  -fPIC)
target_include_directories(imgui PUBLIC imgui)
target_link_libraries(imgui PRIVATE SDL3::SDL3)
