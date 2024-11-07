#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal; // interpolation of normals?!

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
  outTexCoord = texCoord;
  outNormal = normal;
}
