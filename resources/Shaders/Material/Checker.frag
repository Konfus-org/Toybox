#include "ShaderBase.glsl"

// Procedural world-space checkerboard surface for the museum floor. Pairs with Pbr.vert. The checker
// is computed from world position, so it stays crisp ("closest") regardless of the mesh UVs, and is
// shaded with PBR for a polished, shiny floor. Parameter lanes (positional float stream):
//   params[0] = color_a (rgba)
//   params[1] = color_b (rgba)
//   params[2] = (cell_size, metallic, roughness, _)
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec3 v_world_position;
layout(location = 3) in vec3 v_world_normal;
layout(location = 4) in vec4 v_world_tangent;
layout(location = 5) in flat uint v_material_id;
layout(location = 6) in flat float v_fade;

layout(location = 0) out vec4 o_color;

void main()
{
    // Screen-door size/LOD fade: thin the surface out as it shrinks on screen (or cross-fades LODs).
    if (tbx_screen_door(v_fade, gl_FragCoord.xy))
        discard;

    uint mid = v_material_id;
    vec4 color_a = tbx_material_param(mid, 0u);
    vec4 color_b = tbx_material_param(mid, 1u);
    vec4 cfg = tbx_material_param(mid, 2u); // (cell_size, metallic, roughness, _)
    float cell_size = max(cfg.x, 0.01);
    float metallic = clamp(cfg.y, 0.0, 1.0);
    float roughness = clamp(cfg.z, 0.04, 1.0);

    vec2 wp = v_world_position.xz;
    float checker = mod(floor(wp.x / cell_size) + floor(wp.y / cell_size), 2.0);
    vec3 albedo = mix(color_a.rgb, color_b.rgb, checker);

    vec3 normal = normalize(v_world_normal);
    vec3 view_direction = normalize(cameraPositionTime.xyz - v_world_position);
    vec3 lit = tbx_shade_pbr(albedo, metallic, roughness, 1.0, normal, v_world_position, view_direction);

    o_color = tbx_debug_stage_color(vec4(tbx_tonemap(lit), 1.0), albedo, normal, v_world_position);
}
