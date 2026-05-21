#version 450

#include "Toybox/ShaderBase.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;
layout(location = 3) in vec2 a_tex_coord;
layout(location = 4) in vec4 a_color;
layout(location = 5) in mat4 a_instance_model;
layout(location = 9) in mat4 a_instance_normal;

layout(location = 0) out vec2 v_tex_coord;
layout(location = 1) out vec4 v_color;

void main()
{
    v_tex_coord = a_tex_coord;
    v_color = a_color;

    gl_Position = u_view_projection * a_instance_model * vec4(a_position, 1.0);
}
