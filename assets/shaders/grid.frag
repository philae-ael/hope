#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outFragColor;

const vec3 baseColor = vec3(0.5);

void main() {
  vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
  vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));
  float targetWidth = min(uvDeriv.x, uvDeriv.y);
  vec2 drawWidth = clamp(vec2(targetWidth), uvDeriv, vec2(0.5));

  vec2 gridUV = abs(fract(uv) * 2.0 - 1.0); // This is distance from the grid!

  // This is the filtering step with a filter of radius 1.5 pixels
  vec2 lineAA = 1.5*uvDeriv;
  vec2 grid = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV); 

  // This colors the grid so that is 
  grid *= clamp(targetWidth / drawWidth, 0.0, 1.0);

  grid = mix(grid, vec2(targetWidth), clamp(uvDeriv*2 - 1, 0.0, 1.0));

  outFragColor = vec4(baseColor, 1.0) * vec4(1.0 - (1 - grid.x)*(1 - grid.y));
}

