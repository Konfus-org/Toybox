#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_final_color;
layout(location = 1) out vec4 o_geometry_preview_color;
layout(location = 2) out vec4 o_albedo;
layout(location = 3) out vec4 o_normal;
layout(location = 4) out vec4 o_depth_preview;
layout(location = 5) out vec4 o_emissive;
layout(location = 6) out vec4 o_material;

in vec4 v_color;
in vec2 v_tex_coord;
in vec3 v_world_position;
in vec3 v_world_normal;
in vec3 v_world_tangent;
in float v_world_tangent_sign;

layout(binding = 0) uniform sampler2D u_textures[5];

void main()
{
    float diffuse_strength = max(u_material_uniforms[1].x, 0.0);
    float specular_strength = max(u_material_uniforms[3].x, 0.0);
    float shininess_strength = max(u_material_uniforms[4].x, 1.0);
    float color_texture_blend = clamp(u_material_uniforms[5].x, 0.0, 1.0);
    vec4 emissive_color = u_material_uniforms[6];
    float emissive_strength = max(u_material_uniforms[7].x, 0.0);
    float alpha_cutoff = clamp(u_material_uniforms[8].x, 0.0, 1.0);
    float transparency_amount = clamp(u_material_uniforms[9].x, 0.0, 1.0);
    float exposure = max(u_material_uniforms[10].x, 0.0);

    vec4 diffuse_sample = texture(u_textures[0], v_tex_coord);
    float texture_weight = diffuse_strength * color_texture_blend;
    vec4 surface_color = v_color * mix(vec4(1.0), diffuse_sample, texture_weight);
    if (surface_color.a < alpha_cutoff)
        discard;

    float surface_alpha = surface_color.a * (1.0 - transparency_amount);
    vec3 emissive_sample = texture(u_textures[4], v_tex_coord).rgb;
    vec3 emissive = emissive_color.rgb * emissive_sample * emissive_strength * exposure;
    vec3 preview_color = clamp(surface_color.rgb + emissive, 0.0, 1.0);

    vec3 normalized_world_normal = normalize(v_world_normal);
    float specular =
        clamp(texture(u_textures[2], v_tex_coord).r * specular_strength, 0.0, 1.0);
    float shininess = clamp(
        texture(u_textures[3], v_tex_coord).r * shininess_strength,
        1.0,
        256.0);
    float depth_preview = 1.0 - pow(clamp(gl_FragCoord.z, 0.0, 1.0), 24.0);

    o_final_color = vec4(preview_color, surface_alpha);
    o_geometry_preview_color = vec4(preview_color, surface_alpha);
    o_albedo = vec4(surface_color.rgb, surface_alpha);
    o_normal = vec4((normalized_world_normal * 0.5) + 0.5, 1.0);
    o_depth_preview = vec4(vec3(depth_preview), 1.0);
    o_emissive = vec4(emissive, exposure);
    o_material = vec4(specular, shininess, 1.0, 1.0);
}
