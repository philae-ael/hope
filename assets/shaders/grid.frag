#version 450


// The aim was to get a look as clean as blender, seem successful enough
// The code has been inspired by:
// - https://www.shadertoy.com/view/XtBfzz
// - https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8
// - https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-22-fast-prefiltered-lines
// - Math

layout(location = 0) in vec3 dir;
layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding=0) uniform camera {
  mat4 projMatrix;
  mat4 cameraFromWorld;
  mat4 worldFromCamera;
  float projectionWidth;
  float projectionHeight;
  float near;
  float far;
};

#define GRID_EXPONENTIAL_FALLOUT 0x1
#define GRID_PROPORTIONAL_WIDTH 0x2

layout(push_constant) uniform pc {
  vec4 baseColor;
  float decay;
  float scale;
  float line_width;
  uint flags;
};

void main() {
  if(line_width <= 0) discard;

  const vec3 planPos = (cameraFromWorld * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  const vec3 planU = normalize((cameraFromWorld * vec4(0.0, 0.0, -1.0, 0.0)).xyz);
  const vec3 planV = normalize((cameraFromWorld * vec4(1.0, 0.0, 0.0, 0.0)).xyz);
  const vec3 planNormal = cross(planU, planV);

  const vec3 ray = normalize(dir);
  float RoN = dot(ray, planNormal);
  float PoN = -dot(planPos, planNormal); // camerapos = vec3(0)
  if (RoN * PoN >= 0) {
    discard;
  }
  
  float d = -PoN / RoN;
  vec3 intersect = vec3(0) + d * ray;

  vec3 uvw = intersect - planPos;
  vec2 uv = vec2(dot(uvw, planU), dot(uvw, planV));
  float distanceFromCenter = length(d * ray  - d * ray * RoN);
  if (distanceFromCenter >= 10 * decay) {
    discard;
  }

  uv /= scale;
  vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
  vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));


  vec2 baseWidth;
  if((flags & GRID_PROPORTIONAL_WIDTH) != 0) {
    baseWidth = vec2(1.0);
  } else {
    baseWidth = uvDeriv;
  }
  // vec2 drawWidth = vec2(0.02);
  vec2 drawWidth = line_width*baseWidth;

  drawWidth = max(drawWidth, uvDeriv);

  vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);

  // This is the filtering step with a filter of radius 1.5 pixels
  vec2 lineAA = 2*uvDeriv;
  vec2 grid = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV); 

  float fallout = 1.0;
  if((flags & GRID_EXPONENTIAL_FALLOUT) != 0) {
    fallout = exp(- distanceFromCenter / decay);
  } else {
    fallout = clamp(decay  / distanceFromCenter, 0.0, 1.0);
  }

  // This colors the grid so that the grid disapear in the distance
  outFragColor = fallout * (1 - (1 - grid.x)* (1 - grid.y)) * baseColor;

  // Late depth test
  vec4 proj = projMatrix * vec4(intersect, 1.0); 
  gl_FragDepth = proj.z / proj.w;

}
