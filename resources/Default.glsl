
// TODO: seperate vert and frag shader into separate files, and implement a shader preprocessor to
// handle includes and such. For now, this is just a simple default shader that can be used for
// testing and as a fallback.
#type vertex
#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec3 a_normal; // TODO: implement normals!
layout(location = 3) in vec2 a_texcoord;

out vec4 v_color;
out vec4 v_vertex_color;
out vec2 v_tex_coord;
out vec3 v_world_pos;
out vec3 v_world_normal;

uniform vec4 u_color = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    v_color = u_color;
    v_vertex_color = a_color;
    v_tex_coord = a_texcoord;
    vec4 world_pos = u_model * vec4(a_position, 1.0);
    mat3 normal_matrix = transpose(inverse(mat3(u_model)));
    v_world_pos = world_pos.xyz;
    v_world_normal = normalize(normal_matrix * a_normal);
    gl_Position = u_view_proj * world_pos;
}

#type fragment
#version 450 core
#include Lights.glsl

layout(location = 0) out vec4 o_color;

in vec4 v_color;
in vec4 v_vertex_color;
in vec2 v_tex_coord;
in vec3 v_world_pos;
in vec3 v_world_normal;

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;
uniform float u_metallic = 0.0;
uniform float u_roughness = 1.0;
uniform vec4 u_emissive = vec4(0.0, 0.0, 0.0, 1.0);
uniform float u_occlusion = 1.0;
uniform int u_has_normal = 0;

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse, v_tex_coord);
    vec3 emissive_color = u_emissive.rgb;
    float occlusion_factor = u_occlusion;
    float metallic = clamp(u_metallic, 0.0, 1.0);
    float roughness = clamp(u_roughness, 0.0, 1.0);
    vec3 normal = normalize(v_world_normal);
    float normal_factor = 1.0;
    if (u_has_normal != 0)
    {
        vec3 normal_sample = texture(u_normal, v_tex_coord).xyz * 2.0 - 1.0;
        normal_factor = clamp(normal_sample.z * 0.5 + 0.5, 0.0, 1.0);
    }
    float surface_factor = (1.0 - metallic * 0.2) * (1.0 - roughness * 0.5);
    vec3 lighting = tbx_compute_lighting(v_world_pos, normal);
    vec3 lit_color = texture_color.rgb * lighting * occlusion_factor * surface_factor;
    lit_color *= mix(0.75, 1.0, normal_factor);
    lit_color += emissive_color;
    o_color = vec4(lit_color, texture_color.a);
}
