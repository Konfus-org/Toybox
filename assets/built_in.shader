#type vertex
#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec3 a_normal; // TODO: implement normals!
layout(location = 3) in vec2 a_texcoord;

out vec4 v_color;
out vec4 v_vertex_color;
out vec3 v_normal;
out vec2 v_tex_coord;

uniform mat4 u_view_proj;
uniform mat4 u_model;
uniform vec4 u_color = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    v_color = u_color;
    v_vertex_color = a_color;
    v_normal = a_normal;
    v_tex_coord = a_texcoord;
    gl_Position = u_view_proj * u_model * vec4(a_position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_color;

in vec4 v_color;
in vec4 v_vertex_color;
in vec3 v_normal; // TODO: implement normals!
in vec2 v_tex_coord;

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;
uniform float u_metallic = 0.0;
uniform float u_roughness = 1.0;
uniform vec4 u_emissive = vec4(0.0, 0.0, 0.0, 1.0);
uniform float u_occlusion = 1.0;

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse, v_tex_coord);
    vec3 emissive_color = u_emissive.rgb;
    float occlusion_factor = u_occlusion;
    vec3 lit_color = texture_color.rgb * occlusion_factor + emissive_color;
    o_color = vec4(lit_color, texture_color.a);
}
