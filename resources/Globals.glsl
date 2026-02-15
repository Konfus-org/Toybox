uniform mat4 u_view_proj;
uniform mat4 u_model;

vec3 tbx_srgb_to_linear(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(2.2));
}

vec3 tbx_linear_to_srgb(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 tbx_tonemap_aces(vec3 color)
{
    // ACES fitted tonemap (Narkowicz 2015)
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}
