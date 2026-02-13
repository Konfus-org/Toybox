#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_texcoord;

out vec3 v_world_normal;
out vec2 v_tex_coord;
out vec4 v_vertex_color;

uniform vec4 u_color = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    mat3 normal_matrix = mat3(transpose(inverse(u_model)));
    v_world_normal = normalize(normal_matrix * a_normal);
    v_tex_coord = a_texcoord;
    v_vertex_color = u_color;
    gl_Position = u_view_proj * world_position;
}
