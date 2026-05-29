#version 450

#include "Toybox/Materials/UnlitMaterial.glsl"

layout(location = 0) in vec2 v_tex_coord;

layout(location = 0) out vec4 o_gbuffer_albedo;
layout(location = 1) out vec4 o_gbuffer_normal;
layout(location = 2) out vec4 o_gbuffer_material;
layout(location = 3) out vec4 o_gbuffer_emissive;

void main()
{
    vec4 color = tbx_build_unlit_color(v_tex_coord);
    if (color.a < 0.5)
    {
        discard;
    }

    o_gbuffer_albedo = vec4(color.rgb, 1.0);
    o_gbuffer_normal = vec4(0.5, 0.5, 1.0, 1.0);
    o_gbuffer_material = vec4(0.0, 1.0, 1.0, 1.0);
    o_gbuffer_emissive = vec4(color.rgb, 1.0);
}
