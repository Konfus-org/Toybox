#include "ShaderBase.glsl"

// Forward+ unlit surface: albedo color * albedo texture * vertex color. params[0] = albedo_color,
// texture slot 0 = albedo_map (declared .mat order). The world position/normal (from Pbr.vert) are
// consumed only by the editor's render-stage debug views.
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec3 v_world_position;
layout(location = 3) in vec3 v_world_normal;
layout(location = 5) in flat uint v_material_id;
layout(location = 6) in flat float v_fade;

layout(location = 0) out vec4 o_color;

void main()
{
    // Screen-door size/LOD fade: thin the surface out as it shrinks on screen (or cross-fades LODs).
    if (tbx_screen_door(v_fade, gl_FragCoord.xy))
        discard;

    vec4 albedo_color = tbx_material_param(v_material_id, 0u);
    vec4 albedo = tbx_sample_material_texture(v_material_id, 0u, v_uv, vec4(1.0));
    vec4 flat_color = albedo_color * albedo * v_color;
    o_color = tbx_debug_stage_color(flat_color, flat_color.rgb, v_world_normal, v_world_position);
}
