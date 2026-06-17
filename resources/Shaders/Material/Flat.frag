#include "ShaderBase.glsl"

// Forward+ unlit surface: albedo color * albedo texture * vertex color. params[0] = albedo_color,
// texture slot 0 = albedo_map (declared .mat order).
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 5) in flat uint v_material_id;

layout(location = 0) out vec4 o_color;

void main()
{
    vec4 albedo_color = tbx_material_param(v_material_id, 0u);
    vec4 albedo = tbx_sample_material_texture(v_material_id, 0u, v_uv, vec4(1.0));
    o_color = albedo_color * albedo * v_color;
}
