#version 460 core
// Toybox builtin UI vertex stage: screen-space (y-down) quads into clip space.
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

uniform vec2 u_screen;
uniform vec2 u_translation;

out vec4 v_color;
out vec2 v_uv;

void main()
{
    const vec2 at = in_position + u_translation;
    v_color = in_color;
    v_uv = in_uv;
    gl_Position =
        vec4(at.x / u_screen.x * 2.0 - 1.0, 1.0 - at.y / u_screen.y * 2.0, 0.0, 1.0);
}
