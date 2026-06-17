#include "Post.glsl"

// Post-processing color grade through a strip (slice) LUT. Authored as a normal material:
//   params lanes (declared .mat order, packed float stream):
//     params[0]   = lut_tint (rgba, multiplied over the graded color)
//     params[1]   = lut_emissive (rgba, added after grading)
//     params[2].x = lut_strength (how far to grade toward the LUT result)
//     params[2].y = blend (grade strength within this effect)
//   texture slot 0 = the strip LUT.
layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

vec2 get_lut_uv(sampler2D lut, vec2 texture_size, float color_size, float slice, vec3 color)
{
    float color_range = max(color_size - 1.0, 1.0);
    vec2 texel = vec2(slice * color_size, 0.0) + vec2(0.5) + color.rg * color_range;
    // LUT assets are authored top-left, while GL texture coordinates start bottom-left.
    texel.y = texture_size.y - texel.y;
    return texel / texture_size;
}

vec3 sample_lut(sampler2D lut, vec3 color)
{
    vec2 texture_size = vec2(textureSize(lut, 0));
    float color_size = texture_size.y;
    float slice_count = floor((texture_size.x / max(color_size, 1.0)) + 0.5);

    bool has_valid_strip_lut = color_size >= 2.0 && slice_count >= 2.0
                               && abs(texture_size.x - (slice_count * color_size)) <= 0.5;
    if (!has_valid_strip_lut)
        return color;

    float color_range = max(color_size - 1.0, 1.0);
    float blue = color.b * color_range;
    float slice0 = floor(blue);
    float slice1 = min(slice0 + 1.0, color_range);

    vec3 graded0 =
        textureLod(lut, get_lut_uv(lut, texture_size, color_size, slice0, color), 0.0).rgb;
    vec3 graded1 =
        textureLod(lut, get_lut_uv(lut, texture_size, color_size, slice1, color), 0.0).rgb;
    return mix(graded0, graded1, fract(blue));
}

void main()
{
    vec4 source = tbx_post_input(v_tex_coord);
    vec4 tint = tbx_post_param(0u);
    vec4 emissive = tbx_post_param(1u);
    vec4 controls = tbx_post_param(2u);
    float lut_weight = max(controls.x, 0.0) * tbx_saturate(controls.y);

    vec3 graded = source.rgb;
    if (lut_weight > 0.0 && tbx_post_has_texture(0u))
    {
        // Saturating the lookup color keeps oversaturated values inside the 3D LUT domain.
        vec3 lut_color = sample_lut(tbx_post_sampler(0u), tbx_saturate(source.rgb));
        graded = mix(source.rgb, lut_color, lut_weight);
    }

    graded = graded * tint.rgb + emissive.rgb;
    o_color = vec4(tbx_post_apply(v_tex_coord, graded), source.a * tint.a);
}
