#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants
{
    vec2 viewport;
} push_constants;

void main()
{
    vec2 normalized = in_position / push_constants.viewport;
    vec2 clip = vec2(normalized.x * 2.0 - 1.0, 1.0 - normalized.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
    out_color = in_color;
}
