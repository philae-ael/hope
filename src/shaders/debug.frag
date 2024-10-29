#version 450

#define SEGMENT_DASHED 0x1
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in uint flags;

layout(location = 0) out vec4 outFragColor;

const float scale = 10;

void main() {
  vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
  vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));

  vec2 x = vec2(max(0.8, uvDeriv.x), 0.5);
  float pattern = 0.0;
  float correction = 0.0;
  vec2 aa = 1.5*uvDeriv;

  if((flags & SEGMENT_DASHED) != 0) {
    correction = smoothstep(0.25 + aa.y, 0.50 - aa.y, uvDeriv.y * scale / (1 - x.y));
    pattern = (1.0 -  fract(uv.y * scale)) * (1 - correction);
  }

  vec2 factor = smoothstep(x + aa, x - aa, vec2(abs(uv.x), pattern));
  outFragColor = mix(1.0, x.y, correction) * factor.x * factor.y * color;
}
