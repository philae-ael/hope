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

find_package(Vulkan REQUIRED)
target_compile_options(imgui PRIVATE)
target_include_directories(imgui PUBLIC imgui)
set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(imgui PRIVATE SDL3::SDL3 Vulkan::Vulkan)

add_library(misc STATIC
  misc/cgltf.c
  misc/stb_image.c
)
target_compile_options(misc PRIVATE)
target_include_directories(misc PUBLIC misc)
set_property(TARGET misc PROPERTY POSITION_INDEPENDENT_CODE ON)
