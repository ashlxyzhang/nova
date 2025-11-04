#version 450
layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;

layout(set = 2, binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_FragColor;

void main() {
    out_FragColor = in_color * texture(tex_sampler, in_uv);
}