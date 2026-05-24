#include "Toybox/ShaderBase.glsl"
#include "Toybox/Lighting/PbrSurface.glsl"

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxPbrMaterialData
{
    vec4 u_albedo_color;

    vec4 u_emissive_color;
    float u_metallic;
    vec3 _tbx_pad_u_metallic;
    float u_roughness;
    vec3 _tbx_pad_u_roughness;
    float u_normal_strength;
    vec3 _tbx_pad_u_normal_strength;
    float u_ao;
    vec3 _tbx_pad_u_ao;
};

layout(binding = TBX_BINDING_ALBEDO_MAP) uniform sampler2D u_albedo_map;
layout(binding = TBX_BINDING_NORMAL_MAP) uniform sampler2D u_normal_map;
layout(binding = TBX_BINDING_METALLIC_ROUGHNESS_MAP) uniform sampler2D u_metallic_roughness_map;
layout(binding = TBX_BINDING_AO_MAP) uniform sampler2D u_ao_map;
layout(binding = TBX_BINDING_EMISSIVE_MAP) uniform sampler2D u_emissive_map;

vec3 tbx_sample_normal(vec2 uv, vec3 normal, vec4 tangent)
{
    vec3 n = normalize(normal);
    vec3 t = normalize(tangent.xyz);
    vec3 b = normalize(cross(n, t) * tangent.w);

    mat3 tbn = mat3(t, b, n);

    vec3 sampled = texture(u_normal_map, uv).xyz;
    sampled = sampled * 2.0 - 1.0;
    sampled.xy *= u_normal_strength;

    return normalize(tbn * sampled);
}

PbrSurface tbx_build_pbr_surface(
    vec3 world_position,
    vec3 world_normal,
    vec4 world_tangent,
    vec2 uv)
{
    vec4 albedo_sample = texture(u_albedo_map, uv);
    vec4 mr_sample = texture(u_metallic_roughness_map, uv);
    vec4 emissive_sample = texture(u_emissive_map, uv);
    float ao_sample = texture(u_ao_map, uv).r;

    PbrSurface surface;

    surface.world_position = world_position;
    surface.normal = tbx_sample_normal(uv, world_normal, world_tangent);
    surface.albedo = albedo_sample.rgb * u_albedo_color.rgb;
    surface.alpha = albedo_sample.a * u_albedo_color.a;

    surface.metallic = clamp(u_metallic * mr_sample.b, 0.0, 1.0);
    surface.roughness = clamp(u_roughness * mr_sample.g, 0.04, 1.0);
    surface.ao = clamp(u_ao * ao_sample, 0.0, 1.0);

    surface.emissive = emissive_sample.rgb * u_emissive_color.rgb;

    return surface;
}
