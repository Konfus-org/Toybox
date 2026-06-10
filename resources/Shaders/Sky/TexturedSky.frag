#include "ShaderBase.glsl"

// Forward+ sky surface. Samples the equirectangular sky textures along the interpolated view
// direction and tints them with the authored sky color + brightness. The day/night cycle blends the
// bright sky (slot 0) toward the dark sky (slot 1) via blend_factor. Parameter lanes (positional
// float stream from Sky.mat, declared order):
//   params[0]   = color (rgba tint)
//   params[1].x = brightness
//   params[1].z = blend_factor (0 = bright/day sky, 1 = dark/night sky)
// Texture slots: 0 = bright (day) sky, 1 = dark (night) sky.
layout(location = 0) in vec3 v_sky_direction;
layout(location = 1) in flat uint v_material_id;

layout(location = 0) out vec4 o_color;

void main()
{
    // Interpolated cube/sphere directions drift off unit length; normalize before the equirect lookup.
    vec3 direction = normalize(v_sky_direction);
    vec2 uv = vec2(
        atan(direction.z, direction.x) * TBX_INV_TAU + 0.5,
        asin(clamp(direction.y, -1.0, 1.0)) * TBX_INV_PI + 0.5);

    vec4 tint = tbx_material_param(v_material_id, 0u);
    vec4 scalars = tbx_material_param(v_material_id, 1u); // (brightness, ambient_mult, blend_factor, _)
    float brightness = scalars.x;
    float blend_factor = tbx_saturate(scalars.z);

    vec4 bright_sky = tbx_sample_material_texture(v_material_id, 0u, uv, vec4(skyColor.rgb, 1.0));
    vec4 dark_sky = tbx_sample_material_texture(v_material_id, 1u, uv, bright_sky);
    vec3 sky = mix(bright_sky.rgb, dark_sky.rgb, blend_factor);

    o_color = vec4(sky * tint.rgb * brightness, 1.0);
}
