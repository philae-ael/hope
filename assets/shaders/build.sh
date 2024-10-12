glslc --target-env=vulkan1.3 -g tri.frag  -o tri.frag.spv
glslc --target-env=vulkan1.3 -g tri.vert -o tri.vert.spv
spirv-link tri.frag.spv tri.vert.spv -o tri.spv
rm tri.frag.spv tri.vert.spv

glslc --target-env=vulkan1.3 -g grid.frag  -o grid.frag.spv
glslc --target-env=vulkan1.3 -g grid.vert -o grid.vert.spv
spirv-link grid.frag.spv grid.vert.spv -o grid.spv
rm grid.frag.spv grid.vert.spv
