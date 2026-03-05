#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec4 v_color;
in vec2 v_tex_coord;
uniform vec4 u_emissive;
uniform float u_alpha_cutoff;
uniform float u_exposure;

uniform sampler2D u_diffuse;

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse, v_tex_coord);
    float alpha_cutoff = clamp(u_alpha_cutoff, 0.0, 1.0);
    if (texture_color.a < alpha_cutoff)
        discard;

    // Unlit path intentionally ignores surface lighting inputs and keeps only base + emissive.
    vec3 unlit_color = texture_color.rgb + u_emissive.rgb;

    float exposure = max(u_exposure, 0.0);
    vec3 mapped = unlit_color * exposure;
    mapped = tbx_linear_to_srgb(mapped);
    o_color = vec4(mapped, texture_color.a);
}
