#version 450
layout(location = 0) in vec3 in_pos;   // World-space position
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

layout(set = 1, binding = 0) uniform Uniforms
{
    mat4 mvp;
} ubo;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;

void main() {
    gl_Position = ubo.mvp * vec4(in_pos, 1.0);
    out_color = in_color;
    out_uv = in_uv;
}