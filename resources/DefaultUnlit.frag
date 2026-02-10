#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec4 v_color;
in vec2 v_tex_coord;

uniform sampler2D u_diffuse;
uniform vec4 u_emissive = vec4(0.0, 0.0, 0.0, 1.0);
uniform float u_exposure = 1.0;

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

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse, v_tex_coord);

    vec3 mapped = tbx_tonemap_aces((texture_color.rgb + u_emissive.rgb) * max(u_exposure, 0.0));
    mapped = pow(mapped, vec3(1.0 / 2.2));
    o_color = vec4(mapped, texture_color.a);
}
