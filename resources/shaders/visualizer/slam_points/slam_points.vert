#version 450

layout(location = 0) in vec4 aPos;   // xyz = world position
layout(location = 1) in vec4 aColor; // rgb = color [0, 1]

layout(location = 0) out vec4 vColor;

layout(set = 1, binding = 0) uniform Uniforms
{
    mat4 mvp;
    float point_size;
} ubo;

void main()
{
    gl_Position = ubo.mvp * vec4(aPos.xyz, 1.0);
    gl_PointSize = ubo.point_size;
    vColor = vec4(aColor.rgb, 1.0);
}
