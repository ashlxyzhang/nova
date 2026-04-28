#version 450

layout(set = 1, binding = 0) uniform Uniforms
{
    mat4 mvp;
    vec4 color;
} ubo;

// Rectangle in the XY plane (z=0), spanning [-1,1] x [-1,1]
const vec3 positions[6] = vec3[6](
    vec3(-1.0, -1.0, 0.0),
    vec3( 1.0, -1.0, 0.0),
    vec3(-1.0,  1.0, 0.0),

    vec3(-1.0,  1.0, 0.0),
    vec3( 1.0, -1.0, 0.0),
    vec3( 1.0,  1.0, 0.0)
);

void main()
{
    gl_Position = ubo.mvp * vec4(positions[gl_VertexIndex], 1.0);
}
