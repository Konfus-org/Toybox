#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;
layout(location = 1) out vec4 o_geometry_color;
layout(location = 2) out vec4 o_gbuffer_albedo;
layout(location = 3) out vec4 o_gbuffer_normal;
layout(location = 4) out vec4 o_gbuffer_depth;

in vec4 v_color;
in vec2 v_tex_coord;
in vec3 v_world_normal;
uniform vec4 u_emissive;
uniform float u_alpha_cutoff;
uniform float u_transparency_amount;
uniform float u_exposure;

uniform sampler2D u_diffuse_map;

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse_map, v_tex_coord);
    float alpha_cutoff = clamp(u_alpha_cutoff, 0.0, 1.0);
    if (texture_color.a < alpha_cutoff)
        discard;
    float surface_alpha = texture_color.a * (1.0 - clamp(u_transparency_amount, 0.0, 1.0));

    vec3 unlit_color = texture_color.rgb + u_emissive.rgb;

    float exposure = max(u_exposure, 0.0);
    vec3 final_color = unlit_color * exposure;
    vec3 display_color = tbx_linear_to_srgb(final_color);
    float dither = tbx_interleaved_gradient_noise(gl_FragCoord.xy) - 0.5;
    display_color += vec3(dither / 255.0);

    vec3 normalized_world_normal = normalize(v_world_normal);
    float depth_visual = 1.0 - pow(clamp(gl_FragCoord.z, 0.0, 1.0), 24.0);

    o_color = vec4(clamp(display_color, 0.0, 1.0), surface_alpha);
    o_geometry_color = vec4(unlit_color, surface_alpha);
    o_gbuffer_albedo = vec4(texture_color.rgb, surface_alpha);
    o_gbuffer_normal = vec4((normalized_world_normal * 0.5) + 0.5, surface_alpha);
    o_gbuffer_depth = vec4(vec3(depth_visual), 1.0);
}
