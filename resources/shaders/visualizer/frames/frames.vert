#version 450

layout(set = 1, binding = 0) uniform Uniforms
{
    mat4 mvp;
} ubo;

layout(location = 0) out vec2 outUV;

// Model-space positions for a quad from (-1,-1,1) to (1,1,1)
const vec3 positions[6] = vec3[6](
    vec3(-1.0, -1.0, 1.0), // Triangle 1: Bottom-left
    vec3( 1.0, -1.0, 1.0), // Triangle 1: Bottom-right
    vec3(-1.0,  1.0, 1.0), // Triangle 1: Top-left
    
    vec3(-1.0,  1.0, 1.0), // Triangle 2: Top-left
    vec3( 1.0, -1.0, 1.0), // Triangle 2: Bottom-right
    vec3( 1.0,  1.0, 1.0)  // Triangle 2: Top-right
);

// Corresponding UV coordinates (texture coordinates) from (0,0) to (1,1)
const vec2 uvs[6] = vec2[6](
    vec2(0.0, 0.0), // Bottom-left
    vec2(1.0, 0.0), // Bottom-right
    vec2(0.0, 1.0), // Top-left
    
    vec2(0.0, 1.0), // Top-left
    vec2(1.0, 0.0), // Bottom-right
    vec2(1.0, 1.0)  // Top-right
);

void main()
{ 
    gl_Position = ubo.mvp * vec4(positions[gl_VertexIndex], 1.0);

    outUV = uvs[gl_VertexIndex];
}
