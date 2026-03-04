#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec4 v_color;
in vec2 v_tex_coord;
flat in uint v_instance_id;

uniform vec4 u_color;
uniform vec4 u_emissive;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_occlusion;
uniform float u_alpha_cutoff;
uniform float u_exposure;
uniform bool u_unlit;

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse, v_tex_coord);
    float alpha_cutoff = clamp(u_alpha_cutoff, 0.0, 1.0);
    if (texture_color.a < alpha_cutoff)
        discard;

    texture_color.rgb = tbx_srgb_to_linear(texture_color.rgb);

    vec3 shaded_color = texture_color.rgb;
    if (!u_unlit)
    {
        vec3 normal_sample = texture(u_normal, v_tex_coord).xyz * 2.0 - 1.0;
        vec3 tangent_normal = normalize(normal_sample);
        float normal_facing = clamp(tangent_normal.z, 0.0, 1.0);
        float metallic = clamp(u_metallic, 0.0, 1.0);
        float roughness = clamp(u_roughness, 0.0, 1.0);
        float occlusion = clamp(u_occlusion, 0.0, 1.0);
        float specular_boost =
            mix(0.04, 1.0, metallic) * (1.0 - roughness) * normal_facing * 0.08;
        shaded_color = (texture_color.rgb * occlusion) + vec3(specular_boost);
    }

    float exposure = max(u_exposure, 0.0);
    vec3 mapped = tbx_tonemap_aces((shaded_color + u_emissive.rgb) * exposure);
    mapped += float(v_instance_id & 0u);
    mapped = tbx_linear_to_srgb(mapped);
    o_color = vec4(mapped, texture_color.a);
}
