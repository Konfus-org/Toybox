#version 460 core
// Toybox builtin post-processing vertex stage: one fullscreen triangle; every PostProcessing
// fragment shader pairs with this and samples u_scene at v_uv.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

out vec2 v_uv;

void main()
{
    v_uv = in_uv;
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
}
