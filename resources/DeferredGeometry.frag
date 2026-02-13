#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_albedo_spec;
layout(location = 1) out vec4 o_normal;
layout(location = 2) out vec4 o_material;

in vec3 v_world_normal;
in vec2 v_tex_coord;
in vec4 v_vertex_color;

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;
uniform vec4 u_emissive = vec4(0.0, 0.0, 0.0, 1.0);
uniform float u_roughness = 1.0;
uniform float u_specular = 1.0;
uniform float u_occlusion = 1.0;
uniform float u_alpha_cutoff = 0.1;

void main()
{
    vec4 texture_color = v_vertex_color * texture(u_diffuse, v_tex_coord);
    if (texture_color.a < u_alpha_cutoff)
        discard;

    vec3 albedo_linear = tbx_srgb_to_linear(texture_color.rgb);

    vec3 normal_sample = texture(u_normal, v_tex_coord).xyz * 2.0 - 1.0;
    vec3 world_normal = normalize(v_world_normal + normal_sample * 0.25);
    vec3 encoded_normal = normalize(world_normal) * 0.5 + 0.5;

    float roughness = clamp(u_roughness, 0.02, 1.0);
    float specular_strength = clamp((1.0 - roughness) * max(u_specular, 0.0), 0.0, 1.0);
    float shininess = mix(4.0, 128.0, 1.0 - roughness);
    vec3 emissive_linear = tbx_srgb_to_linear(u_emissive.rgb) * max(u_occlusion, 0.0);

    o_albedo_spec = vec4(albedo_linear, specular_strength);
    o_normal = vec4(encoded_normal, 1.0);
    o_material = vec4(emissive_linear, shininess / 128.0);
}
