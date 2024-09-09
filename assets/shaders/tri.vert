#version 450

layout(location = 0) in vec3 position;
layout (location = 0) out vec3 outColor;

void main() 
{
	const vec3 colors[3] = vec3[3](
		vec3(1.0f, 0.0f, 0.0f), //red
		vec3(0.0f, 1.0f, 0.0f), //green
		vec3(00.f, 0.0f, 1.0f)  //blue
	);

	gl_Position = vec4(position, 1.0f);
	outColor = colors[gl_VertexIndex];
}