glslc --target-env=vulkan1.3 -g tri.frag  -o tri.frag.spv
glslc --target-env=vulkan1.3 -g tri.vert -o tri.vert.spv
spirv-link tri.frag.spv tri.vert.spv -o tri.spv
