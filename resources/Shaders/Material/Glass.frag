#include "ShaderBase.glsl"

// Reflective glass. A pure multiply filter can only ever darken the framebuffer, so it can never show
// a highlight — to actually reflect the flashlight back this uses standard src-over alpha blending:
// the rgb is the pane's own reflected light (sky/ambient reflection plus every light's specular lobe,
// shadowed exactly like an opaque surface, so the flashlight glints off the glass) and the alpha is
// its opacity. Opacity stays low (see-through) when looking straight through the pane and rises toward
// 1 at grazing angles (Fresnel) and wherever a specular highlight lands, so reflections read as bright
// glints on an otherwise clear pane. The reflection is tinted by albedo through the metallic f0, so a
// blue pane throws a blue-tinted reflection. Pairs with Pbr.vert. Parameter lanes (declared .mat order):
//   params[0] = albedo_color (rgb = glass/reflection tint)
//   params[2] = (metallic, roughness, normal_strength, ao)
// Texture slot 0 (albedo) optionally modulates the tint (e.g. a stained-glass pattern).
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec3 v_world_position;
layout(location = 3) in vec3 v_world_normal;
layout(location = 4) in vec4 v_world_tangent;
layout(location = 5) in flat uint v_material_id;

layout(location = 0) out vec4 o_color;

void main()
{
    uint mid = v_material_id;
    vec4 tint =
        v_color * tbx_material_param(mid, 0u) * tbx_sample_material_texture(mid, 0u, v_uv, vec4(1.0));
    vec4 scalars = tbx_material_param(mid, 2u);
    float metallic = clamp(scalars.x, 0.0, 1.0);
    float roughness = max(scalars.y, 0.02);
    float ao = scalars.w;

    vec3 view_direction = normalize(cameraPositionTime.xyz - v_world_position);
    vec3 normal = normalize(v_world_normal);
    // Two-sided pane: light whichever face is actually being rasterized (matches Pbr.frag).
    if (!gl_FrontFacing)
        normal = -normal;

    // Reflected light off the glass: ambient + per-light specular (including the flashlight), with the
    // same directional/local shadowing as opaque surfaces. The albedo tints f0 (via metallic), so the
    // glint takes on the glass color.
    vec3 reflected =
        tbx_shade_pbr(tint.rgb, metallic, roughness, ao, normal, v_world_position, view_direction);
    vec3 color = tbx_tonemap(reflected);

    // Opacity: a low clear base, a Fresnel rim, and the highlight itself — so the pane is see-through
    // except where it reflects, where it turns bright and opaque (the visible glint).
    float ndotv = tbx_saturate(dot(normal, view_direction));
    float fresnel = pow(1.0 - ndotv, 5.0);
    float highlight = max(max(color.r, color.g), color.b);
    float opacity = tbx_saturate(0.08 + fresnel * 0.9 + highlight * 0.6);

    // Keep a faint colored-glass cast on whatever still shows through the clear part of the pane.
    color += tint.rgb * 0.06;

    // The albedo color's alpha is the pane's overall presence: 1 is the fully-realised glass (the look
    // above), lower values a thinner, more see-through pane, 0 effectively no glass at all. The alpha
    // blend is a colored composite (final = src.rgb * (src.a + dst)), so presence has to drive both
    // halves — scale the reflected glint (src.a) down with it, and fade the colored filter (src.rgb)
    // toward white, an identity filter that leaves the background untouched, as the pane thins out.
    // At alpha = 1 both are no-ops, so existing glass renders exactly as before.
    float presence = tbx_saturate(tint.a);
    opacity *= presence;
    color = mix(vec3(1.0), color, presence);
    o_color = vec4(color, opacity);
}
