#include "ShaderBase.glsl"

// Forward+ PBR surface. Reads its packed material record (params in declared .mat order) and shades
// itself against the clustered light list. Parameter lanes (positional float stream):
//   params[0] = albedo_color (rgba)
//   params[1] = emissive_color (rgba)
//   params[2] = (metallic, roughness, normal_strength, ao)
// Texture slots (declared .mat order): 0 albedo, 1 normal, 2 metallic, 3 roughness, 4 ao, 5 emissive.
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
    vec4 albedo_color = tbx_material_param(mid, 0u);
    vec4 emissive_color = tbx_material_param(mid, 1u);
    vec4 scalars = tbx_material_param(mid, 2u);
    float normal_strength = scalars.z;

    vec4 base = v_color * albedo_color * tbx_sample_material_texture(mid, 0u, v_uv, vec4(1.0));

    vec3 view_direction = normalize(cameraPositionTime.xyz - v_world_position);
    vec3 normal = normalize(v_world_normal);
    // Two-sided surfaces (e.g. museum interior walls) render both faces; flip the geometric normal
    // to the side actually being rasterized so the back face lights correctly. Keying off
    // gl_FrontFacing (not the view vector) keeps lighting tied to geometry, so a directional light
    // outside a wall no longer "leaks" onto the interior face the way a view-dependent flip did.
    if (!gl_FrontFacing)
        normal = -normal;
    if (tbx_material_has_texture(mid, 1u))
    {
        vec3 tangent = normalize(v_world_tangent.xyz);
        vec3 bitangent = normalize(cross(normal, tangent) * v_world_tangent.w);
        vec3 sampled = tbx_sample_material_texture(mid, 1u, v_uv, vec4(0.5, 0.5, 1.0, 1.0)).xyz * 2.0 - 1.0;
        sampled.xy *= normal_strength;
        normal = normalize(mat3(tangent, bitangent, normal) * sampled);
    }

    float metallic = clamp(scalars.x * tbx_sample_material_texture(mid, 2u, v_uv, vec4(1.0)).r, 0.0, 1.0);
    float roughness = scalars.y * tbx_sample_material_texture(mid, 3u, v_uv, vec4(1.0)).r;
    float ao = clamp(scalars.w * tbx_sample_material_texture(mid, 4u, v_uv, vec4(1.0)).r, 0.0, 1.0);
    vec3 emissive = emissive_color.rgb * tbx_sample_material_texture(mid, 5u, v_uv, vec4(1.0)).rgb;

    vec3 lit = tbx_shade_pbr(base.rgb, metallic, roughness, ao, normal, v_world_position, view_direction);

    o_color = tbx_debug_stage_color(
        vec4(tbx_tonemap(lit + emissive), base.a), base.rgb, normal, v_world_position);
}
