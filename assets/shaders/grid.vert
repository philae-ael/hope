#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec3 dir;

layout(set = 0, binding=0) uniform camera {
  mat4 projMatrix;
  mat4 cameraFromWorld;
  mat4 worldFromCamera;
  float projectionWidth;
  float projectionHeight;
  float near;
  float far;
};

const vec2 uv[] = {
   vec2(+1.0, +1.0),
   vec2(+1.0, -1.0),
   vec2(-1.0, -1.0),
   vec2(+1.0, +1.0),
   vec2(-1.0, -1.0),
   vec2(-1.0, +1.0),
 };

void main() {
  vec2 position = uv[gl_VertexIndex];
	gl_Position = vec4(position, 0.0, 1.0);
  dir = vec3(vec2(projectionWidth, projectionHeight) * uv[gl_VertexIndex], -far);
}
