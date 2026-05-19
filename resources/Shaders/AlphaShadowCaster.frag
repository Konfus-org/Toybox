#version 450

#include "Toybox/ShaderBase.glsl"

layout(location = 0) in vec2 v_tex_coord;

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxShadowMaterialData
{
    vec4 u_color;

    // x = alpha_cutoff
    // yzw = unused
    vec4 u_params0;
};

layout(binding = TBX_BINDING_ALBEDO_MAP)
uniform sampler2D u_main_tex;

#define u_alpha_cutoff u_params0.x

void main()
{
    float alpha = texture(u_main_tex, v_tex_coord).a * u_color.a;

    if (alpha < u_alpha_cutoff)
    {
        discard;
    }

    // Depth written automatically.
}
