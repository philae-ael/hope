#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outFragColor;
layout(set = 1, binding = 0) uniform sampler2D sampledColor[];

layout(push_constant) uniform pc {
   layout(offset=64) int sampler_idx;
};

void main() 
{
  vec4 baseColor = texture(sampledColor[sampler_idx], inTexCoord);
  if(baseColor.a < 0.5) {
    discard;
  }
	outFragColor = vec4(inColor * baseColor.rgb, 1.0);
}

