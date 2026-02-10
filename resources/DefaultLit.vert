#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_texcoord;

out vec4 v_color;
out vec2 v_tex_coord;
out vec3 v_world_pos;
out vec3 v_world_normal;

uniform vec4 u_color = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    v_color = u_color;
    v_tex_coord = a_texcoord;
    vec4 world_pos = u_model * vec4(a_position, 1.0);
    v_world_pos = world_pos.xyz;
    v_world_normal = normalize(u_normal_matrix * a_normal);
    gl_Position = u_view_proj * world_pos;
}
