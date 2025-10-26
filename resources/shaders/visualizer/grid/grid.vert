#version 450

layout(location = 0) in vec3 aPos;

layout(location = 0) out vec4 vColor;

layout(set = 1, binding = 0) uniform Uniforms
{
    mat4 mvp;
} ubo;

void main()
{
    gl_Position = ubo.mvp * vec4(aPos.xyz, 1.0);
    vColor = vec4(0.0, 0.0, 0.0, 1.0);
}