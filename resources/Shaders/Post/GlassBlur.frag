#include "Post.glsl"

// Frosted-glass backdrop for the editor's overlay cards: box-blurs the scene inside up to seven
// normalized rectangles (the card footprints the editor pushes per view via view.setGlass), leaving
// every other pixel untouched. The cards themselves draw editor-side with a translucent tint over
// this, which is what completes the glass.
//   params lane 0      = vec4(rect count, blur radius in pixels, 0, 0)
//   params lane 1..7   = one rect each: vec4(x, y, width, height), normalized, top-left origin.
layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

const vec2 TBX_GLASS_TAPS[8] = vec2[8](
    vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
    vec2(0.7071, 0.7071), vec2(0.7071, -0.7071), vec2(-0.7071, 0.7071), vec2(-0.7071, -0.7071));

void main()
{
    vec4 scene = tbx_post_input(v_tex_coord);
    vec4 meta = tbx_post_param(0u);
    int count = int(meta.x + 0.5);
    float radius = max(meta.y, 1.0);

    // The editor authors rects with a top-left origin (its overlay space); the scene uv origin is
    // bottom-left, so flip v for the containment test only.
    vec2 point = vec2(v_tex_coord.x, 1.0 - v_tex_coord.y);
    bool inside = false;
    for (int i = 0; i < 7; ++i)
    {
        if (i >= count)
            break;
        vec4 rect = tbx_post_param(uint(i + 1));
        if (point.x >= rect.x && point.x <= rect.x + rect.z
            && point.y >= rect.y && point.y <= rect.y + rect.w)
        {
            inside = true;
            break;
        }
    }

    if (!inside)
    {
        o_color = scene;
        return;
    }

    // Two rings of eight taps: cheap, and the frost tint the card draws on top hides the low tap
    // count.
    vec2 texel = tbx_post_texel();
    vec3 total = scene.rgb;
    float weight = 1.0;
    for (int i = 0; i < 8; ++i)
    {
        vec2 offset = TBX_GLASS_TAPS[i] * texel * radius;
        total += tbx_post_input(v_tex_coord + offset).rgb;
        total += tbx_post_input(v_tex_coord + offset * 0.45).rgb;
        weight += 2.0;
    }

    o_color = vec4(mix(scene.rgb, total / weight, tbx_saturate(tbx_post_blend())), scene.a);
}
