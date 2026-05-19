#version 450

#include "Toybox/Post/PostProcessBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxTonemapData
{
    float u_exposure;
    vec3 _tbx_pad_u_exposure;
    float u_gamma;
    vec3 _tbx_pad_u_gamma;
};

void main()
{
    vec3 hdr = texture(u_source_color, v_tex_coord).rgb;
    vec3 color = tbx_apply_exposure_tonemap_gamma(hdr, u_exposure, u_gamma);

    o_color = vec4(color, 1.0);
}
