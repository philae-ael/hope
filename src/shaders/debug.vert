#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 pos_start;
layout(location = 2) in vec3 pos_end;
layout(location = 3) in float width;
layout(location = 4) in uint flags;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) flat out uint out_flags;

layout(set = 0, binding=0) uniform camera {
  mat4 projMatrix;
  mat4 cameraFromWorld;
  mat4 worldFromCamera;
  float projectionWidth;
  float projectionHeight;
  float near;
  float far;
  float screen_height;
  float screen_width;
};

const vec2 uvs[] = {
   vec2(+1.0, +1.0),
   vec2(+1.0, -1.0),
   vec2(-1.0, -1.0),
   vec2(+1.0, +1.0),
   vec2(-1.0, -1.0),
   vec2(-1.0, +1.0),
 };

void main() {
  vec2 uv = uvs[gl_VertexIndex];

  vec4 proj_dir = vec4(pos_end - pos_start, 0.0) / 2;
  vec4 pos =  projMatrix * cameraFromWorld * (vec4((pos_start + pos_end) / 2, 1.0) + uv.y * proj_dir);
  float w = max(width, 2 * pos.w / screen_width); // min 1px

  vec2 dir_projected = normalize((projMatrix * cameraFromWorld * proj_dir).xy);
  gl_Position = pos + uv.x * w * vec4(dir_projected.y, -dir_projected.x, 0.0, 0.0);

  out_uv = uv;
  out_flags = flags;
  out_color = vec4(vec3(width/w), 1) * color;
}
