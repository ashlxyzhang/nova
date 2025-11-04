#version 450

// Input from our vertex buffer
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

// Uniform data sent from C++
// 'set = 1' is used for vertex shader uniforms in SDL_gpu
layout(set = 1, binding = 0) uniform Uniforms 
{
    mat4 mvp;
} ubo;

// Output to the fragment shader
layout(location = 0) out vec4 vColor;

void main() 
{
    // Set the final position and pass the color through
    gl_Position = ubo.mvp * vec4(aPos, 1.0);
    vColor = aColor;
}