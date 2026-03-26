#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;

out vec4 v_color;
out vec3 v_world_normal;

uniform vec4 u_color;

void main()
{
    v_color = u_color;

    vec4 world_position = tbx_get_model_matrix(u_model) * vec4(a_position, 1.0);
    mat3 normal_matrix = mat3(transpose(inverse(tbx_get_model_matrix(u_model))));
    v_world_normal = normalize(normal_matrix * a_normal);

    gl_Position = u_view_proj * world_position;
}
