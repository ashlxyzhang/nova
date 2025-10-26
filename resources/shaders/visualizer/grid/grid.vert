#version 450

layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aColor;

layout(location = 0) out vec4 vColor;

layout(set = 1, binding = 0) uniform Uniforms
{
    mat4 mvp;
} ubo;

void main()
{
    gl_Position = ubo.mvp * vec4(aPos.xyz, 1.0);
    vColor = aColor;
}