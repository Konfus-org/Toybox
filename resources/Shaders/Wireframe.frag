#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_final_color;
layout(location = 1) out vec4 o_geometry_preview_color;
layout(location = 2) out vec4 o_albedo;
layout(location = 3) out vec4 o_normal;
layout(location = 4) out vec4 o_depth_preview;
layout(location = 5) out vec4 o_emissive;
layout(location = 6) out vec4 o_material;

in vec4 g_color;
in vec3 g_world_normal;
in vec3 g_barycentric;

uniform vec4 u_emissive;
uniform float u_exposure;
uniform float u_wireframe_width;

void main()
{
    vec3 edge_distance = fwidth(g_barycentric) * max(u_wireframe_width, 1.0);
    vec3 edge_mask = step(g_barycentric, edge_distance);
    float edge_alpha = max(edge_mask.x, max(edge_mask.y, edge_mask.z));
    if (edge_alpha < 0.5)
        discard;

    vec3 unlit_color = g_color.rgb + u_emissive.rgb;
    float exposure = max(u_exposure, 0.0);
    vec3 exposed_unlit_color = unlit_color * exposure;
    vec3 display_color = tbx_linear_to_srgb(exposed_unlit_color);
    float dither = tbx_interleaved_gradient_noise(gl_FragCoord.xy) - 0.5;
    display_color += vec3(dither / 255.0);

    vec3 normalized_world_normal = normalize(g_world_normal);
    float depth_preview = 1.0 - pow(clamp(gl_FragCoord.z, 0.0, 1.0), 24.0);

    o_final_color = vec4(clamp(display_color, 0.0, 1.0), 1.0);
    o_geometry_preview_color = vec4(unlit_color, 1.0);
    o_albedo = vec4(g_color.rgb, 1.0);
    o_normal = vec4((normalized_world_normal * 0.5) + 0.5, 1.0);
    o_depth_preview = vec4(vec3(depth_preview), 1.0);
    o_emissive = vec4(unlit_color, exposure);
    o_material = vec4(0.0, 1.0, 1.0, 1.0);
}
