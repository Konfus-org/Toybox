#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_texcoord;
layout(location = 8) in mat4 a_model;
layout(location = 12) in uint a_instance_id;

out vec4 v_color;
out vec2 v_tex_coord;
flat out uint v_instance_id;

uniform vec4 u_color = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    v_color = u_color;
    v_tex_coord = a_texcoord;
    v_instance_id = tbx_get_instance_id(a_instance_id);
    gl_Position = u_view_proj * (tbx_get_model_matrix(a_model) * vec4(a_position, 1.0));
}
