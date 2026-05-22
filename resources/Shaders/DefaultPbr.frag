#version 450

#include "Toybox/Materials/PbrMaterial.glsl"

layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_world_normal;
layout(location = 2) in vec4 v_world_tangent;
layout(location = 3) in vec2 v_tex_coord;

layout(location = 0) out vec4 o_gbuffer_albedo;
layout(location = 1) out vec4 o_gbuffer_normal;
layout(location = 2) out vec4 o_gbuffer_material;
layout(location = 3) out vec4 o_gbuffer_emissive;

void main()
{
    PbrSurface surface = tbx_build_pbr_surface(
        v_world_position,
        normalize(v_world_normal),
        v_world_tangent,
        v_tex_coord);

    o_gbuffer_albedo = vec4(surface.albedo, surface.alpha);
    o_gbuffer_normal = vec4(normalize(surface.normal) * 0.5 + 0.5, 1.0);
    o_gbuffer_material = vec4(surface.metallic, surface.roughness, surface.ao, 1.0);
    o_gbuffer_emissive = vec4(surface.emissive, 1.0);
}
