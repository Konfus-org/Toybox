#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;
layout(location = 1) out vec4 o_geometry_color;
layout(location = 2) out vec4 o_gbuffer_albedo;
layout(location = 3) out vec4 o_gbuffer_normal;
layout(location = 4) out vec4 o_gbuffer_depth;
layout(location = 5) out vec4 o_gbuffer_emissive;
layout(location = 6) out vec4 o_gbuffer_material;

in vec4 v_color;
in vec2 v_tex_coord;
in vec3 v_world_normal;

uniform vec4 u_color;
uniform vec4 u_emissive;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_occlusion;
uniform float u_alpha_cutoff;
uniform float u_exposure;

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_diffuse, v_tex_coord);
    float alpha_cutoff = clamp(u_alpha_cutoff, 0.0, 1.0);
    if (texture_color.a < alpha_cutoff)
        discard;

    float metallic = clamp(u_metallic, 0.0, 1.0);
    float roughness = clamp(u_roughness, 0.0, 1.0);
    float occlusion = clamp(u_occlusion, 0.0, 1.0);
    float exposure = max(u_exposure, 0.0);

    vec3 normalized_world_normal = normalize(v_world_normal);
    vec3 preview_light_direction = normalize(vec3(0.35, 0.8, 0.45));
    float preview_n_dot_l = max(dot(normalized_world_normal, preview_light_direction), 0.0);
    vec3 preview_color =
        (texture_color.rgb * ((0.25 + (preview_n_dot_l * 0.75)) * occlusion))
        + (u_emissive.rgb * 0.5);
    float depth_visual = 1.0 - pow(clamp(gl_FragCoord.z, 0.0, 1.0), 24.0);

    o_color = vec4(preview_color, texture_color.a);
    o_geometry_color = vec4(preview_color, texture_color.a);
    o_gbuffer_albedo = vec4(texture_color.rgb, texture_color.a);
    o_gbuffer_normal = vec4((normalized_world_normal * 0.5) + 0.5, 1.0);
    o_gbuffer_depth = vec4(vec3(depth_visual), 1.0);
    o_gbuffer_emissive = vec4(u_emissive.rgb, 1.0);
    o_gbuffer_material = vec4(metallic, roughness, occlusion, exposure);
}
