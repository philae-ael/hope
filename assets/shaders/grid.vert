#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec2 outTexCoord;

layout(set = 0, binding=0) uniform camera {
  mat4 projMatrix;
  mat4 cameraFromLocal;
};

const vec3 positions[] = {
   vec3(+1.0, 0.0, +1.0),
   vec3(+1.0, 0.0, -1.0),
   vec3(-1.0, 0.0, -1.0),
   vec3(+1.0, 0.0, +1.0),
   vec3(-1.0, 0.0, -1.0),
   vec3(-1.0, 0.0, +1.0),
 };

const vec2 uv[] = {
   vec2(1.0, 1.0),
   vec2(1.0, 0.0),
   vec2(0.0, 0.0),
   vec2(1.0, 1.0),
   vec2(0.0, 0.0),
   vec2(0.0, 1.0),
 };

const float scale = 100;

void main() 
{
  vec3 position =  positions[gl_VertexIndex];
	gl_Position = projMatrix  * cameraFromLocal * vec4(scale * position.xyz, 1.0f);
  outTexCoord = scale * uv[gl_VertexIndex];
}
