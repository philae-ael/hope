find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

function(compile_shader target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "ENV;OUTPUT_DIRECTORY" "ARGS;SOURCES")
  foreach(source ${arg_SOURCES})
    set(input  ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    cmake_path(GET input FILENAME SOURCE_FILENAME)
    add_custom_command(
            OUTPUT ${arg_OUTPUT_DIRECTORY}${SOURCE_FILENAME}.spv
            DEPENDS ${source}
            DEPFILE ${source}.d
            COMMAND
                ${glslc_executable}
                $<$<BOOL:${arg_ENV}>:--target-env=${arg_ENV}>
                ${arg_ARGS}
                -MD -MF ${source}.d
                -o ${arg_OUTPUT_DIRECTORY}${SOURCE_FILENAME}.spv
                ${input}
        )
    target_sources(${target} PRIVATE  ${arg_OUTPUT_DIRECTORY}/${SOURCE_FILENAME}.spv)
  endforeach()
endfunction()
