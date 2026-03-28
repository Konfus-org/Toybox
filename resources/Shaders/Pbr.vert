#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_texcoord;
layout(location = 4) in vec4 a_tangent;

out vec4 v_color;
out vec2 v_tex_coord;
out vec3 v_world_position;
out vec3 v_world_normal;
out vec3 v_world_tangent;
out float v_world_tangent_sign;

uniform vec4 u_color;

void main()
{
    v_color = u_color;
    v_tex_coord = a_texcoord;

    vec4 world_position = tbx_get_model_matrix(u_model) * vec4(a_position, 1.0);
    v_world_position = world_position.xyz;
    mat3 normal_matrix = mat3(transpose(inverse(tbx_get_model_matrix(u_model))));
    v_world_normal = normalize(normal_matrix * a_normal);
    v_world_tangent = normalize(normal_matrix * a_tangent.xyz);
    v_world_tangent_sign = a_tangent.w;

    gl_Position = u_view_proj * world_position;
}
