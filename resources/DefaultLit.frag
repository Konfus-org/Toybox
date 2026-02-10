#version 450 core
#include Globals.glsl
#include Lights.glsl

layout(location = 0) out vec4 o_color;

in vec4 v_color;
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
uniform float u_exposure = 1.0;

vec3 tbx_tonemap_aces(vec3 color)
{
    // ACES fitted tonemap (Narkowicz 2015)
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

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

    vec3 albedo = max(texture_color.rgb, vec3(0.0));
    vec3 view_dir = normalize(u_camera_pos - v_world_pos);

    vec3 lit_color =
        tbx_compute_pbr_lighting(v_world_pos, normal, view_dir, albedo, metallic, roughness);
    lit_color *= occlusion_factor;
    lit_color *= mix(0.75, 1.0, normal_factor);
    lit_color += emissive_color;

    vec3 mapped = tbx_tonemap_aces(lit_color * max(u_exposure, 0.0));
    mapped = pow(mapped, vec3(1.0 / 2.2));
    o_color = vec4(mapped, texture_color.a);
}
