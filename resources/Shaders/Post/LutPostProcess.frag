#include "Base/PostShaderBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

layout(std140, binding = 5) uniform TbxLutPostData
{
    vec4 u_lut_tint;
    vec4 u_lut_emissive;
    vec4 u_lut_strength;
    vec4 u_blend;
};

vec2 get_lut_uv(vec2 texture_size, float color_size, float slice, vec3 color)
{
    float color_range = max(color_size - 1.0, 1.0);
    vec2 texel = vec2(slice * color_size, 0.0) + vec2(0.5) + color.rg * color_range;
    // LUT assets are authored top-left, while GL texture coordinates start bottom-left.
    texel.y = texture_size.y - texel.y;
    return texel / texture_size;
}

vec3 sample_lut(vec3 color)
{
    vec2 texture_size = vec2(textureSize(tbx_post_effect_texture0, 0));
    float color_size = texture_size.y;
    float slice_count = floor((texture_size.x / max(color_size, 1.0)) + 0.5);

    bool has_valid_strip_lut = color_size >= 2.0
        && slice_count >= 2.0
        && abs(texture_size.x - (slice_count * color_size)) <= 0.5;
    if (!has_valid_strip_lut)
    {
        return color;
    }

    float color_range = max(color_size - 1.0, 1.0);
    float blue = color.b * color_range;
    float slice0 = floor(blue);
    float slice1 = min(slice0 + 1.0, color_range);

    vec3 graded0 =
        textureLod(
            tbx_post_effect_texture0,
            get_lut_uv(texture_size, color_size, slice0, color),
            0.0)
            .rgb;
    vec3 graded1 =
        textureLod(
            tbx_post_effect_texture0,
            get_lut_uv(texture_size, color_size, slice1, color),
            0.0)
            .rgb;
    return mix(graded0, graded1, fract(blue));
}

void main()
{
    vec4 tint = u_lut_tint;
    vec4 emissive = u_lut_emissive;
    float lut_weight = max(u_lut_strength.x, 0.0) * tbx_saturate(u_blend.x);
    vec4 source = texture(tbx_gbuffer_final_color, v_tex_coord);

    vec3 graded = source.rgb;
    if (lut_weight > 0.0)
    {
        // Saturating the lookup color keeps oversaturated HDR values inside the 3D LUT domain.
        vec3 lut_color = sample_lut(tbx_saturate(source.rgb));
        graded = mix(source.rgb, lut_color, lut_weight);
    }

    graded *= tint.rgb;
    o_color = tbx_write_to_final_color(graded + emissive.rgb, source.a * tint.a);
}
