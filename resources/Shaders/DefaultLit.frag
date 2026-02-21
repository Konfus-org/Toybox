#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec4 v_color;
in vec2 v_tex_coord;

uniform sampler2D u_diffuse;
uniform vec4 u_emissive = vec4(0.0, 0.0, 0.0, 1.0);
uniform float u_exposure = 1.0;

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse, v_tex_coord);
    texture_color.rgb = tbx_srgb_to_linear(texture_color.rgb);

    vec3 mapped = tbx_tonemap_aces((texture_color.rgb + u_emissive.rgb) * max(u_exposure, 0.0));
    mapped = tbx_linear_to_srgb(mapped);
    o_color = vec4(mapped, texture_color.a);
}
