cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --ld-path=/usr/bin/mold")
# SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --ld-path=/usr/bin/mold")

project("app")

function(target_compile_link_options target)
  target_compile_options(${target} ${ARGN})
  target_link_options(${target} ${ARGN})
endfunction()

find_package(SDL2 REQUIRED CONFIG)
find_package(Vulkan REQUIRED)

add_library(core OBJECT
  src/core/debug.cpp
  src/core/log.cpp
  src/core/math.cpp
  src/core/memory.cpp
  src/core/platform.cpp
  src/core/string.cpp
  src/core/type_info.cpp
  src/os/time.cpp
  src/os/memory.cpp
  src/subsystems.cpp
  src/vulkan/init.cpp
  src/vulkan/vulkan.cpp
  src/vulkan/frame.cpp

)

if (UNIX)
  target_sources(core PRIVATE
    src/os/linux/time.cpp
    src/os/linux/memory.cpp
  )
endif()

target_compile_options(core PUBLIC -O0 -g -fPIC)
target_compile_options(core PUBLIC -Wall -Wextra -Wconversion)
target_compile_options(core PUBLIC -Wno-unused-parameter -Wno-missing-field-initializers)
target_compile_options(core PUBLIC -DDEBUG)
target_compile_link_options(core PUBLIC -fno-omit-frame-pointer)
# target_compile_link_options(core PUBLIC -fsanitize=address )
target_compile_link_options(core PUBLIC -flto)
target_compile_link_options(core PUBLIC -fvisibility=hidden)

# target_compile_options(core PRIVATE -DARENA_DEBUG )
# target_compile_options(core PRIVATE -DMEM_DEBUG )
# target_compile_options(core PRIVATE -DMEM_USE_MALLOC)
# target_compile_options(core PRIVATE -DSCRATCH_DEBUG )

target_link_libraries(core PUBLIC SDL2::SDL2 Vulkan::Vulkan stdc++exp)


add_library(app MODULE
  src/app.cpp
)
target_link_libraries(app PRIVATE core)
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:app> libapp-hotreloadable.so
)
set_property(
  TARGET app
  PROPERTY ADDITIONAL_CLEAN_FILES libapp_*.so
)


add_executable(main
  src/main.cpp
  src/app_loader.cpp
)
target_link_libraries(main PRIVATE core)
