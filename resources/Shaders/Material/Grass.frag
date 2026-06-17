#include "ShaderBase.glsl"

// Fluffy grass surface, the carving half of the shell technique (pairs with Grass.geom + Pbr.vert).
// Grass.geom stacks the ground triangle into shells along the normal; this shader keeps the ground
// shell solid and, on every shell above it, discards the gaps so only soft round tufts survive.
// Each world cell grows one tuft: a rounded strand that stays full at the base and rounds off (rather
// than spiking to a point) toward the tip, shaded with a smooth view-facing dome normal so it reads
// as a soft, fluffy clump instead of a hard blade. The lawn stays OPAQUE (depth-correct from every
// angle); the fluff comes from the rounded silhouette, heavy tuft overlap and the soft dome shading,
// not from alpha — there's no MSAA/TAA here, so feathered alpha edges aren't available without
// sacrificing self-occlusion.
// Parameter lanes (positional float stream, declared .mat order):
//   params[0] = base_color (rgba grass tint)
//   params[1] = (blade_scale, fuzz, roughness, height)
layout(location = 0) in vec3 g_world_position;
layout(location = 1) in vec3 g_world_normal;
layout(location = 2) in flat uint g_material_id;
layout(location = 3) in float g_shell; // 0 at the ground, 1 at the tallest tuft tip

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

    // Each grid cell grows one tuft; xz is shared across shells (extrusion is purely vertical).
    float density = blade_scale * 4.0; // tufts per world unit
    vec2 cell = floor(g_world_position.xz * density);
    vec2 local = fract(g_world_position.xz * density) - 0.5;

    vec2 root = (grass_hash2(cell) - 0.5) * 0.7;      // tuft root jitter inside the cell
    float tuft_height = mix(0.5, 1.0, grass_hash(cell + 11.0)); // normalized tip height
    float tint = grass_hash(cell + 37.0);

    bool is_ground = g_shell <= 0.001;
    float along = clamp(g_shell / max(tuft_height, 0.001), 0.0, 1.0); // 0 at root, 1 at tip

    // Round tuft cross-section: full at the base, rounding off toward the tip (a cosine cap, not a
    // sharp point) so the clump stays plump and soft. Radius > 0.5 at the base makes neighbouring
    // tufts overlap into a continuous fluffy carpet that thins out as it rises.
    vec2 d = local - root;
    float dist = length(d);
    float radius = mix(0.58, 0.12, smoothstep(0.0, 1.0, along)) * sqrt(max(1.0 - along * along, 0.0));
    float fringe = max(radius * 0.5, 0.001);
    float coverage = 1.0 - smoothstep(radius - fringe, radius, dist); // 1 in the core, 0 past the edge
    if (!is_ground)
    {
        if (g_shell > tuft_height) // this shell is above the tuft's tip
            discard;
        if (coverage <= 0.02) // outside the tuft
            discard;
    }

    // Smooth dome normal: each tuft shades like a soft little hemisphere instead of a flat blade.
    // It points up at the base, leans outward toward the fringe (driven by the in-cell offset), and
    // borrows a little of the view direction so the fluff stays soft and evenly lit from any angle.
    vec3 view_direction = normalize(cameraPositionTime.xyz - g_world_position);
    vec3 dome = vec3(d.x, mix(1.5, 0.5, along) + 0.4, d.y) * vec3(fuzz + 0.5, 1.0, fuzz + 0.5);
    vec3 normal = normalize(g_world_normal * 0.6 + normalize(dome) + view_direction * 0.2);

    // Soft self-shadow: darker and more occluded at the roots and toward the tuft's edge, brighter and
    // sun-bleached at the tips. The edge fade (coverage) rounds the lighting off so the fluff reads
    // soft rather than hard-cut.
    float ao = is_ground ? 0.5 : mix(0.5, 1.15, along) * mix(0.75, 1.0, coverage);
    vec3 grass = base.rgb * mix(0.85, 1.15, tint) * ao;
    grass = mix(grass, grass * vec3(0.95, 1.1, 0.8), tint * 0.4); // subtle per-tuft hue scatter

    vec3 lit = tbx_shade_pbr(grass, 0.0, roughness, 1.0, normal, g_world_position, view_direction);
    // Gentle translucent fill so the tips glow a touch and the lawn reads soft, not hard-lit.
    lit += grass * ambientLight.rgb * (along * 0.25);

    o_color = vec4(tbx_tonemap(lit), 1.0);
}
