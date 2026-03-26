#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_texcoord;

out vec4 v_color;
out vec2 v_tex_coord;
out vec3 v_world_normal;

uniform vec4 u_color;
uniform float u_alpha_cutoff;
uniform float u_exposure;

void main()
{
    v_color = u_color;
    v_tex_coord = a_texcoord;

    vec4 world_position = tbx_get_model_matrix(u_model) * vec4(a_position, 1.0);
    mat3 normal_matrix = mat3(transpose(inverse(tbx_get_model_matrix(u_model))));
    v_world_normal = normalize(normal_matrix * a_normal);

    gl_Position = u_view_proj * world_position;
}
