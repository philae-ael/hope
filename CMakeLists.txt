cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --ld-path=/usr/bin/mold")
# SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --ld-path=/usr/bin/mold")

project("app")
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

function(target_compile_link_options target)
  target_compile_options(${target} ${ARGN})
  target_link_options(${target} ${ARGN})
endfunction()

find_package(Vulkan REQUIRED)
add_subdirectory(vendor SYSTEM)

add_library(core
  src/core/core/debug.cpp
  src/core/core/log.cpp
  src/core/core/math.cpp
  src/core/core/memory.cpp
  src/core/core/platform.cpp
  src/core/core/string.cpp
  src/core/core/type_info.cpp
  src/core/containers/sync.cpp
  src/core/os/time.cpp
  src/core/os/memory.cpp
  src/core/vulkan/init.cpp
  src/core/vulkan/vulkan.cpp
  src/core/vulkan/frame.cpp
  src/core/vulkan/vma.cpp
  src/core/vulkan/subsystem.cpp
  src/core/vulkan/image.cpp
  src/core/fs/fs.cpp
  src/core/debug/time.cpp
)
target_include_directories(core PUBLIC src)

if (UNIX)
  target_sources(core PRIVATE
    src/core/os/linux/time.cpp
    src/core/os/linux/memory.cpp
  )
endif()
target_compile_options(core PUBLIC -O3 -g -fPIC)
target_compile_options(core PUBLIC -Wall -Wextra -Wconversion)
target_compile_options(core PUBLIC -Wno-unused-parameter -Wno-missing-field-initializers)
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

target_link_libraries(core PUBLIC SDL3::SDL3 Vulkan::Vulkan stdc++exp imgui GPUOpen::VulkanMemoryAllocator)


add_executable(loader
  src/loader/main.cpp
)

if (BUILD_SHARED_LIBS)
  target_compile_definitions(core PUBLIC -DSHARED=1)

  add_library(app MODULE)
  target_sources(loader PRIVATE src/loader/app_loader-shared.cpp)
else()
  add_library(app OBJECT)
  target_sources(loader PRIVATE src/loader/app_loader-static.cpp)
  target_link_libraries(loader PRIVATE app)
endif()
target_link_libraries(app PUBLIC core)

target_sources(app PRIVATE
  src/app/app.cpp
  src/app/renderer.cpp
  src/app/testfree.cpp
  src/app/imgui_renderer.cpp
  src/app/triangle_renderer.cpp
)

target_link_libraries(loader PRIVATE core)
