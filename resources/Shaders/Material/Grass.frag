#include "ShaderBase.glsl"

// Fuzzy grass surface, the carving half of the shell technique (pairs with Grass.geom + Pbr.vert).
// Grass.geom stacks the ground triangle into shells along the normal; this shader keeps the ground
// shell solid and, on every shell above it, discards the gaps so only tapered blade strands survive.
// Stacked across the shells those strands read as real 3D blades that hold up at a grazing angle.
// Parameter lanes (positional float stream, declared .mat order):
//   params[0] = base_color (rgba grass tint)
//   params[1] = (blade_scale, fuzz, roughness, height)
layout(location = 0) in vec3 g_world_position;
layout(location = 1) in vec3 g_world_normal;
layout(location = 2) in flat uint g_material_id;
layout(location = 3) in float g_shell; // 0 at the ground, 1 at the tallest blade tip

layout(location = 0) out vec4 o_color;

float grass_hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

vec2 grass_hash2(vec2 p)
{
    return vec2(grass_hash(p), grass_hash(p + 19.19));
}

void main()
{
    uint mid = g_material_id;
    vec4 base = tbx_material_param(mid, 0u);
    vec4 cfg = tbx_material_param(mid, 1u); // (blade_scale, fuzz, roughness, height)
    float blade_scale = max(cfg.x, 0.5);
    float fuzz = clamp(cfg.y, 0.0, 1.0);
    float roughness = clamp(cfg.z, 0.05, 1.0);

    // Each grid cell grows one blade; xz is shared across shells (extrusion is purely vertical).
    float density = blade_scale * 4.0; // blades per world unit
    vec2 cell = floor(g_world_position.xz * density);
    vec2 local = fract(g_world_position.xz * density) - 0.5;

    vec2 root = (grass_hash2(cell) - 0.5) * 0.7;      // blade root jitter inside the cell
    float blade_height = mix(0.45, 1.0, grass_hash(cell + 11.0)); // normalized tip height
    float tint = grass_hash(cell + 37.0);

    bool is_ground = g_shell <= 0.001;
    float along = g_shell / max(blade_height, 0.001); // 0 at root, 1 at this blade's tip

    // Blade cross-section: a round strand that tapers to a point toward the tip.
    float radius = mix(0.40, 0.04, clamp(along, 0.0, 1.0));
    float dist = length(local - root);
    if (!is_ground)
    {
        if (g_shell > blade_height) // this shell is above the blade's tip
            discard;
        if (dist > radius) // this pixel is in the gap between blades
            discard;
    }

    // Dark at the roots (fake self-shadow / AO between strands), bright at the tips -> the fuzzy look.
    float occlusion = is_ground ? 0.45 : mix(0.4, 1.2, along);
    vec3 grass = base.rgb * mix(0.75, 1.2, tint) * occlusion;
    grass = mix(grass, grass * vec3(0.95, 1.1, 0.8), tint * 0.5); // subtle per-blade hue scatter

    // Tilt the normal per blade so grazing sunlight shimmers across the lawn.
    vec3 normal = normalize(g_world_normal + vec3(root.x, 0.0, root.y) * fuzz);
    vec3 view_direction = normalize(cameraPositionTime.xyz - g_world_position);
    vec3 lit = tbx_shade_pbr(grass, 0.0, roughness, 1.0, normal, g_world_position, view_direction);

    o_color = vec4(tbx_tonemap(lit), 1.0);
}
