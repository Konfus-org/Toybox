#include "ShaderBase.glsl"

// Forward+ alpha-tested (cutout) PBR surface: samples the albedo texture and discards fragments
// whose alpha falls below the cutoff, then shades the kept fragments with PBR. Pairs with Pbr.vert.
// Parameter lanes (positional float stream, declared .mat order):
//   params[0] = albedo_color (rgba)
//   params[1] = (metallic, roughness, ao, alpha_cutoff)
// Texture slots: 0 = albedo (its alpha channel drives the cutout).
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
    vec4 scalars = tbx_material_param(mid, 1u);

    vec4 albedo = v_color * albedo_color * tbx_sample_material_texture(mid, 0u, v_uv, vec4(1.0));
    if (albedo.a < scalars.w)
        discard;

    float metallic = tbx_saturate(scalars.x);
    float roughness = scalars.y;
    float ao = tbx_saturate(scalars.z);
    vec3 normal = normalize(v_world_normal);
    vec3 view_direction = normalize(cameraPositionTime.xyz - v_world_position);
    if (!gl_FrontFacing) // two-sided cutout: shade the face actually being rasterized
        normal = -normal;
    vec3 lit = tbx_shade_pbr(albedo.rgb, metallic, roughness, ao, normal, v_world_position, view_direction);

    o_color = vec4(tbx_tonemap(lit), 1.0);
}
