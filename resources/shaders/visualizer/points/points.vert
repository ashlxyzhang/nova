#version 450

layout(location = 0) in vec4 aPos;

layout(location = 0) out vec4 vColor;

layout(set = 1, binding = 0) uniform Uniforms
{
    mat4 mvp;
    vec4 negative_color;
    vec4 positive_color;
    float point_size;
} ubo;

void main()
{
    gl_Position = ubo.mvp * vec4(aPos.xyz, 1.0);
    gl_PointSize = ubo.point_size;
    vColor = aPos.w > 0.0 ? ubo.positive_color : ubo.negative_color;
}