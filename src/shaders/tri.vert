#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;

layout(set = 0, binding=0) uniform camera {
  mat4 projMatrix;
  mat4 cameraFromWorld;
};

layout(push_constant) uniform pc {
  mat4 WorldFromLocal;
};

void main() 
{
	gl_Position = projMatrix  * cameraFromWorld * WorldFromLocal * vec4(position.xyz, 1.0f);
	outColor = (0.2 + 0.8*clamp(dot(normal, normalize(vec3(5.0, 5.0, 0.0) - position.xyz)), 0.0, 1.0)) * vec3(1.0,1.0,1.0);
  outTexCoord = texCoord;
}
