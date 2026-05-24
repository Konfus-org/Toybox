#include "Toybox/ShaderBase.glsl"

layout(binding = TBX_BINDING_GBUFFER_ALBEDO) uniform sampler2D u_gbuffer_albedo;
layout(binding = TBX_BINDING_GBUFFER_NORMAL) uniform sampler2D u_gbuffer_normal;
layout(binding = TBX_BINDING_GBUFFER_MATERIAL) uniform sampler2D u_gbuffer_material;
layout(binding = TBX_BINDING_GBUFFER_EMISSIVE) uniform sampler2D u_gbuffer_emissive;
layout(binding = TBX_BINDING_GBUFFER_DEPTH) uniform sampler2D u_gbuffer_depth;
layout(binding = TBX_BINDING_GBUFFER_FINAL_COLOR) uniform sampler2D u_gbuffer_final_color;

vec3 tbx_tonemap_aces(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

vec3 tbx_apply_exposure_tonemap_gamma(vec3 color, float exposure, float gamma)
{
    color *= exposure;
    color = tbx_tonemap_aces(color);
    color = tbx_linear_to_display(color, gamma);

    return color;
}
