#version 460 core
// Toybox builtin shadow-depth vertex stage.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

uniform mat4 u_model;
uniform mat4 u_light_view_projection;

void main()
{
    gl_Position = u_light_view_projection * u_model * vec4(in_position, 1.0);
}
