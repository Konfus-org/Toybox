#include "Post.glsl"

// Selection outline: edge-detect the tag mask (tbx_tag_mask) and composite a solid outline
// over the scene color. An outer-edge pixel is one that is NOT itself masked but borders a masked
// pixel, so the outline hugs the outside of the tagged silhouette.
//   params lane 0   = outline color (rgb); a zero color falls back to a warm orange.
//   params lane 1.x = outline thickness in pixels (>= 1).
layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

const vec2 TBX_OUTLINE_DIRS[8] = vec2[8](
    vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
    vec2(1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(-1.0, -1.0));

void main()
{
    vec4 scene = tbx_post_input(v_tex_coord);
    vec2 texel = tbx_post_texel();

    vec3 outline_color = tbx_post_param(0u).rgb;
    if (dot(outline_color, vec3(1.0)) <= 0.0)
        outline_color = vec3(1.0, 0.55, 0.12);
    float thickness = max(tbx_post_param(1u).x, 1.0);

    float center = texture(tbx_tag_mask, v_tex_coord).r;
    float neighbor = 0.0;
    for (int i = 0; i < 8; ++i)
    {
        vec2 sample_uv = v_tex_coord + TBX_OUTLINE_DIRS[i] * texel * thickness;
        neighbor = max(neighbor, texture(tbx_tag_mask, sample_uv).r);
    }

    // Outer edge: unmasked here, masked next door. Scaled by the effect's stack blend weight.
    float edge = step(0.5, neighbor) * (1.0 - step(0.5, center)) * tbx_saturate(tbx_post_blend());
    o_color = vec4(mix(scene.rgb, outline_color, edge), scene.a);
}
