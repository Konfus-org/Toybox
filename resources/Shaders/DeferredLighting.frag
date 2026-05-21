#version 450

#include "Toybox/Lighting/PbrLighting.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

layout(binding = TBX_BINDING_GBUFFER_ALBEDO) uniform sampler2D u_gbuffer_albedo;
layout(binding = TBX_BINDING_GBUFFER_NORMAL) uniform sampler2D u_gbuffer_normal;
layout(binding = TBX_BINDING_GBUFFER_MATERIAL) uniform sampler2D u_gbuffer_material;
layout(binding = TBX_BINDING_GBUFFER_EMISSIVE) uniform sampler2D u_gbuffer_emissive;
layout(binding = TBX_BINDING_GBUFFER_DEPTH) uniform sampler2D u_gbuffer_depth;

PbrSurface tbx_reconstruct_pbr_surface_from_gbuffer(vec2 uv)
{
    vec4 albedo_sample = texture(u_gbuffer_albedo, uv);
    vec4 normal_sample = texture(u_gbuffer_normal, uv);
    vec4 material_sample = texture(u_gbuffer_material, uv);
    vec4 emissive_sample = texture(u_gbuffer_emissive, uv);
    float depth = texture(u_gbuffer_depth, uv).r;

    PbrSurface surface;
    surface.world_position = tbx_reconstruct_world_position(uv, depth);
    surface.normal = tbx_unpack_normal(normal_sample.xyz);
    surface.albedo = albedo_sample.rgb;
    surface.alpha = albedo_sample.a;
    surface.metallic = clamp(material_sample.r, 0.0, 1.0);
    surface.roughness = clamp(material_sample.g, 0.04, 1.0);
    surface.ao = clamp(material_sample.b, 0.0, 1.0);
    surface.emissive = emissive_sample.rgb;

    return surface;
}

void main()
{
    float depth = texture(u_gbuffer_depth, v_tex_coord).r;
    if (depth >= 1.0)
    {
        discard;
    }

    PbrSurface surface = tbx_reconstruct_pbr_surface_from_gbuffer(v_tex_coord);
    o_color = vec4(max(tbx_shade_pbr(surface), vec3(0.0)), surface.alpha);
}
