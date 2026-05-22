#version 450

#include "Toybox/Post/PostProcessBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

void main()
{
    vec3 hdr = texture(u_gbuffer_final_color, v_tex_coord).rgb;
    vec3 color = hdr * 1.0;
    color = tbx_tonemap_aces(color);

    o_color = tbx_write_to_final_color(color, 1.0);
}
