#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 0) uniform sampler2DArray frame;

layout(set = 3, binding = 0) uniform FrameData
{
    // x : first frames timestamp
    // y : second frames timestamp
    // z : current timestamp
    // w : unused
    vec4 timestamps;
} frame_data;

void main()
{
    // Create a new UV coordinate mirrored on the vertical axis
    // over and underflows will mirror repeat, based on sampler settings
    vec2 mirroredUV = vec2(inUV.x + 1.0, inUV.y);

    if (frame_data.timestamps.x > 0.0 && frame_data.timestamps.y > 0.0)
    {
        // Use the new mirroredUV for sampling
        vec4 lower = texture(frame, vec3(mirroredUV, 0));
        vec4 upper = texture(frame, vec3(mirroredUV, 1));
        float val = (frame_data.timestamps.z - frame_data.timestamps.x) / (frame_data.timestamps.y - frame_data.timestamps.x);
        FragColor = mix(lower, upper, val);
    }
    else
    {
        // Also use the new mirroredUV here
        FragColor = texture(frame, vec3(mirroredUV, 0));
    }
}
