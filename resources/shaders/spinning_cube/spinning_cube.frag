#version 450

// Input from the vertex shader
layout(location = 0) in vec4 vColor;

// Final output color
layout(location = 0) out vec4 FragColor;

void main() 
{
    FragColor = vColor;
}