#version 460 core
// Toybox unlit validation stage (docs/RenderFailures.md): broken renderables draw loud and
// full-strength, ignoring scene lighting. Layout everywhere: position(3) + normal(3) + uv(2).
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

uniform mat4 u_model;
uniform mat4 u_view_projection;

out vec2 v_uv;

void main()
{
    v_uv = in_uv;
    gl_Position = u_view_projection * u_model * vec4(in_position, 1.0);
}
