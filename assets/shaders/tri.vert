#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 outColor;

layout(push_constant) uniform pc {
  mat4 projMatrix;
  mat4 cameraFromLocal;
  mat4 localFromWorld;
};

void main() 
{
	gl_Position = projMatrix  * cameraFromLocal* localFromWorld* vec4(position.xyz, 1.0f);
	outColor = (0.2 + 0.8*clamp(dot(normal, normalize(vec3(5.0, 5.0, 0.0) - position.xyz)), 0.0, 1.0)) * vec3(1.0,1.0,1.0);
}
