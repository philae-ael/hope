find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

function(compile_shader target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "ENV;OUTPUT_DIRECTORY" "ARGS;SOURCES")
  foreach(source ${arg_SOURCES})
    cmake_path(SET source "${source}")
    cmake_path(GET source PARENT_PATH SOURCE_DIRECTORY)
    cmake_path(SET input  "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    cmake_path(GET input FILENAME SOURCE_FILENAME)
    cmake_path(SET target_src "${arg_OUTPUT_DIRECTORY}/${SOURCE_FILENAME}.spv")
    add_custom_command(
            OUTPUT ${arg_OUTPUT_DIRECTORY}${SOURCE_FILENAME}.spv
            DEPENDS ${source}
            DEPFILE ${source}.d
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SOURCE_DIRECTORY}
            COMMAND
                ${glslc_executable}
                $<$<BOOL:${arg_ENV}>:--target-env=${arg_ENV}>
                ${arg_ARGS}
                -MD -MF ${source}.d
                -o ${target_src}
                ${input}
        )
    target_sources(${target} PRIVATE  ${target_src})
  endforeach()
endfunction()
