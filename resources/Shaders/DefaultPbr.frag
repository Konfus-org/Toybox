#version 450

#include "Toybox/Lighting/PbrLighting.glsl"

layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_world_normal;
layout(location = 2) in vec4 v_world_tangent;
layout(location = 3) in vec2 v_tex_coord;
layout(location = 4) in vec4 v_color;

layout(location = 0) out vec4 o_color;

void main()
{
    PbrSurface surface = tbx_build_pbr_surface(
        v_world_position,
        normalize(v_world_normal),
        v_world_tangent,
        v_tex_coord);

    surface.albedo *= v_color.rgb;
    surface.alpha *= v_color.a;

    vec3 color = pow(max(tbx_shade_pbr(surface), vec3(0.0)), vec3(1.0 / 2.2));
    o_color = vec4(color, surface.alpha);
}
