set(SDL_AUDIO OFF)
set(SDL_VIDEO   ON)
set(SDL_GPU ON)
set(SDL_RENDER OFF)
set(SDL_CAMERA OFF)
set(SDL_JOYSTICK ON)
set(SDL_HAPTIC OFF)
set(SDL_HIDAPI ON)
set(SDL_POWER OFF)
set(SDL_SENSOR OFF)
set(SDL_DIALOG OFF)
add_subdirectory(SDL)
add_subdirectory(VulkanMemoryAllocator)
add_subdirectory(libuv)

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
target_compile_options(misc PRIVATE -O3)
target_include_directories(misc PUBLIC misc)
set_property(TARGET misc PROPERTY POSITION_INDEPENDENT_CODE ON)
