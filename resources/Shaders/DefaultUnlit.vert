#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 3) in vec2 a_texcoord;

out vec4 v_color;
out vec2 v_tex_coord;

uniform vec4 u_color;

void main()
{
    v_color = u_color;
    v_tex_coord = a_texcoord;
    gl_Position = u_view_proj * (tbx_get_model_matrix(u_model) * vec4(a_position, 1.0));
}
