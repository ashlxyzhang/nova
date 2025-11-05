#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 0) uniform sampler2DArray frame;

layout(set = 3, binding = 0) uniform FrameData
{
    // x : first frames timestep
    // y : second frames timestep
    // z : current timestep
    // w : unused
    vec4 timesteps;
} frame_data;

void main()
{
    FragColor = texture(frame, vec3(inUV, 0));
}
