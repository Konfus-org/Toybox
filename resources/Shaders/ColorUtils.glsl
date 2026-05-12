vec3 tbx_srgb_to_linear(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(2.2));
}

vec3 tbx_linear_to_srgb(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

float tbx_interleaved_gradient_noise(vec2 pixel_coord)
{
    return fract(52.9829189 * fract(dot(pixel_coord, vec2(0.06711056, 0.00583715))));
}