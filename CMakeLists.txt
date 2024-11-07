cmake_minimum_required(VERSION 3.25)

include(cmake/shader.cmake)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --ld-path=/usr/bin/mold")
# SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --ld-path=/usr/bin/mold")

project("app")
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

function(target_compile_link_options target)
  target_compile_options(${target} ${ARGN})
  target_link_options(${target} ${ARGN})
endfunction()

find_package(Vulkan REQUIRED)
add_subdirectory(vendor SYSTEM)

add_library(core
  src/core/containers/sync.cpp
  src/core/core/debug.cpp
  src/core/core/log.cpp
  src/core/core/memory.cpp
  src/core/core/platform.cpp
  src/core/core/string.cpp
  src/core/core/type_info.cpp
  src/core/fs/fs.cpp
  src/core/math/math.cpp
  src/core/os/memory.cpp
  src/core/os/time.cpp
)
target_include_directories(core PUBLIC src)


if (UNIX)
  target_sources(core PRIVATE
    src/core/os/linux/time.cpp
    src/core/os/linux/memory.cpp
    src/core/os/linux/fs.cpp
  )
else()
  target_sources(core PRIVATE
    src/core/os/windows/time.cpp
    src/core/os/windows/memory.cpp
    src/core/os/windows/fs.cpp
  )
endif()

if (MSVC)
  target_compile_options(core PUBLIC /Zc:preprocessor /std:c++latest )
else()
  target_compile_link_options(core PUBLIC -fdiagnostics-color=always)
  target_compile_options(core PUBLIC -O0 -g -std=c++23)
  target_compile_options(core PUBLIC -msse  -msse2  -msse3)

  target_compile_options(core PUBLIC -Wall -Wextra -Wconversion)
  target_compile_options(core PUBLIC -Wno-unused-parameter -Wno-missing-field-initializers -Wno-missing-braces)
  target_link_options(core PUBLIC -rdynamic)
  target_compile_link_options(core PUBLIC -fno-omit-frame-pointer)
  # target_compile_link_options(core PUBLIC -fsanitize=address )
  target_compile_link_options(core PUBLIC -flto)
  target_compile_link_options(core PUBLIC -fvisibility=hidden)

  target_compile_definitions(core PUBLIC -DDEBUG)
  target_compile_definitions(core PUBLIC -DVMA_DEBUG)
  # target_compile_definitions(core PRIVATE -DARENA_DEBUG )
  # target_compile_definitions(core PRIVATE -DMEM_DEBUG )
  # target_compile_definitions(core PRIVATE -DMEM_USE_MALLOC)
  # target_compile_definitions(core PRIVATE -DSCRATCH_DEBUG )
endif()

if(WIN32) 
  target_compile_definitions(core PUBLIC -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX )
endif()

set_property(TARGET core PROPERTY POSITION_INDEPENDENT_CODE ON)
# set_property(TARGET core PROPERTY UNITY_BUILD ON)

if (UNIX)
  target_link_libraries(core PUBLIC stdc++exp)
endif()
  target_link_libraries(core PUBLIC libuv::libuv)

add_library(engine
  src/engine/graphics/subsystem.cpp
  src/engine/graphics/vulkan/frame.cpp
  src/engine/graphics/vulkan/image.cpp
  src/engine/graphics/vulkan/init.cpp
  src/engine/graphics/vulkan/timings.cpp
  src/engine/graphics/vulkan/vma.cpp
  src/engine/graphics/vulkan/vulkan.cpp
  src/engine/utils/config.cpp
  src/engine/utils/time.cpp
)
target_link_libraries(engine PUBLIC core SDL3::SDL3-static Vulkan::Vulkan imgui GPUOpen::VulkanMemoryAllocator misc)
set_property(TARGET engine PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(engine PUBLIC src)


add_executable(loader src/loader/main.cpp)

if (BUILD_SHARED_LIBS)
  target_compile_definitions(core PUBLIC -DSHARED=1)

  add_library(app MODULE)
  target_sources(loader PRIVATE src/loader/app_loader-shared.cpp)
else()
  add_library(app OBJECT)
  target_sources(loader PRIVATE src/loader/app_loader-static.cpp)
  target_link_libraries(loader PRIVATE app)
endif()
target_link_libraries(app PUBLIC core engine)

target_sources(app PRIVATE
  src/app/app.cpp
  src/app/renderer.cpp
  src/app/imgui_renderer.cpp
  src/app/basic_renderer.cpp
  src/app/profiler.cpp
  src/app/mesh.cpp
)

add_library(shaders INTERFACE)
compile_shader(shaders
    ENV vulkan1.3
    FORMAT bin
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders-compiled/
    ARGS -I src/shaders/include -g
    SOURCES
      src/shaders/grid.frag
      src/shaders/grid.vert
      src/shaders/debug.frag
      src/shaders/debug.vert
      src/shaders/tri.frag
      src/shaders/tri.vert
)

target_link_libraries(loader PRIVATE core engine shaders)

add_executable(testcore
  src/tests/tests.cpp
  src/tests/math.cpp
  src/tests/indexmap.cpp
  src/tests/tuple.cpp
  src/tests/stable_vec.cpp
)
target_link_libraries(testcore PRIVATE core engine)
target_compile_options(testcore PRIVATE -Wno-format-zero-length -Wno-format-security)
