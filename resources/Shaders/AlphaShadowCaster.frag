#version 450

#include "Toybox/ShaderBase.glsl"

layout(location = 0) in vec2 v_tex_coord;

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxShadowMaterialData
{
    vec4 u_color;
    float u_alpha_cutoff;
    vec3 _tbx_pad_u_alpha_cutoff;
};

layout(binding = TBX_BINDING_ALBEDO_MAP) uniform sampler2D u_albedo_map;

void main()
{
    float alpha = texture(u_albedo_map, v_tex_coord).a * u_color.a;

    if (alpha < u_alpha_cutoff)
    {
        discard;
    }

    // Depth written automatically.
}
