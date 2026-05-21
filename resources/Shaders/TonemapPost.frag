#version 450

#include "Toybox/Post/PostProcessBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

void main()
{
    vec3 hdr = texture(u_source_color, v_tex_coord).rgb;
    vec3 color = tbx_apply_exposure_tonemap_gamma(hdr, 1.0, 2.2);

    o_color = vec4(color, 1.0);
}
