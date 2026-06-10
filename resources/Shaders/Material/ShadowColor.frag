#include "ShaderBase.glsl"

// Writes a transparent caster's transmittance into the colored shadow map. The pass uses
// multiplicative blending (dst *= src) against a white-cleared target, so stacked transparent
// layers accumulate and the forward pass tints the directional light by the result. Transmittance
// trends from white (clear) toward the material's albedo tint as its opacity rises.
layout(location = 0) in flat uint v_material_id;

layout(location = 0) out vec4 o_color;

void main()
{
    vec4 albedo = tbx_material_param(v_material_id, 0u);
    vec3 transmittance = clamp(mix(vec3(1.0), albedo.rgb, albedo.a), 0.0, 1.0);
    o_color = vec4(transmittance, 1.0);
}
