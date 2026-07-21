#version 460 core
// Toybox builtin sky vertex stage: one fullscreen triangle pinned to the far plane.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

out vec2 v_ndc;

void main()
{
    v_ndc = in_position.xy;
    gl_Position = vec4(in_position.xy, 1.0, 1.0);
}
