#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outFragColor;
layout(set = 1, binding = 0) uniform sampler2D sampledColor[];

layout(push_constant) uniform pc {
   layout(offset=64) int sampler_idx;
};

void main() 
{
  const vec3 lightDir  = normalize(vec3(1.0, 1.0, 0.1));
  vec4 baseColor = texture(sampledColor[sampler_idx], inTexCoord);
  if(baseColor.a < 0.5) {
    discard;
  }

  float factor = dot(inNormal, lightDir);
	outFragColor = (0.5 + 0.5 * factor) * vec4(baseColor.rgb, 1.0);
}

