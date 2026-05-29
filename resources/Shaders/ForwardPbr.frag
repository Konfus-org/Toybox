#version 450

#include "Toybox/Materials/PbrMaterial.glsl"
#include "Toybox/Lighting/PbrLighting.glsl"

layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_world_normal;
layout(location = 2) in vec4 v_world_tangent;
layout(location = 3) in vec2 v_tex_coord;

layout(location = 0) out vec4 o_color;

void main()
{
    PbrSurface surface = tbx_build_pbr_surface_from_fragment(
        v_world_position,
        v_world_normal,
        v_world_tangent,
        v_tex_coord,
        gl_FrontFacing);

    vec3 color = max(tbx_shade_pbr(surface), vec3(0.0));
    o_color = vec4(color, surface.alpha);
}
