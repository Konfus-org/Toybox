#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_texcoord;

out vec4 v_color;
out vec2 v_tex_coord;
flat out uint v_instance_id;

uniform vec4 u_color;
uniform vec4 u_emissive;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_occlusion;
uniform float u_alpha_cutoff;
uniform float u_exposure;

void main()
{
    v_color = u_color;
    v_tex_coord = a_texcoord;
    v_instance_id = tbx_get_instance_id(0u);
    gl_Position = u_view_proj * (tbx_get_model_matrix(u_model) * vec4(a_position, 1.0));
}
