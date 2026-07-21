#version 460 core
// The health bar's custom ui vertex stage: a sine wobble over screen position.
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

uniform vec2 u_screen;
uniform vec2 u_translation;
uniform float u_time;

out vec4 v_color;
out vec2 v_uv;

void main()
{
    vec2 at = in_position + u_translation;
    at.y += sin(u_time * 5.0 + at.x * 0.06) * 3.0;
    v_color = in_color;
    v_uv = in_uv;
    gl_Position =
        vec4(at.x / u_screen.x * 2.0 - 1.0, 1.0 - at.y / u_screen.y * 2.0, 0.0, 1.0);
}
