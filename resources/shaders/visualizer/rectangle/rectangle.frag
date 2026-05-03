#version 450

layout(set = 3, binding = 0) uniform Uniforms
{
    mat4 mvp;
    vec4 color;
} ubo;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = ubo.color;
}
