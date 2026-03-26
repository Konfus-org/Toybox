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

uniform vec4 u_color;
uniform vec4 u_emissive;
struct MaterialSurface
{
    vec4 primary;
    vec4 secondary;
    vec4 map_strengths;
};

layout(std140, binding = 9) uniform MaterialSurfaceBlock
{
    MaterialSurface u_material_surface;
};

uniform sampler2D u_diffuse_map;
uniform sampler2D u_normal_map;
uniform sampler2D u_specular_map;
uniform sampler2D u_shininess_map;
uniform sampler2D u_emissive_map;
uniform sampler2D u_occlusion_map;

float tbx_apply_map_strength(float neutral_value, float sampled_value, float strength)
{
    return neutral_value + ((sampled_value - neutral_value) * max(strength, 0.0));
}

vec3 tbx_apply_map_strength(vec3 neutral_value, vec3 sampled_value, float strength)
{
    return neutral_value + ((sampled_value - neutral_value) * max(strength, 0.0));
}

void main()
{
    float specular_base = u_material_surface.primary.x;
    float shininess_base = u_material_surface.primary.y;
    float occlusion_base = u_material_surface.primary.z;
    float alpha_cutoff = clamp(u_material_surface.primary.w, 0.0, 1.0);
    float transparency_amount = clamp(u_material_surface.secondary.x, 0.0, 1.0);
    float exposure = max(u_material_surface.secondary.y, 0.0);
    float diffuse_strength = max(u_material_surface.secondary.z, 0.0);
    float normal_strength = max(u_material_surface.secondary.w, 0.0);
    float specular_map_strength = max(u_material_surface.map_strengths.x, 0.0);
    float shininess_map_strength = max(u_material_surface.map_strengths.y, 0.0);
    float emissive_map_strength = max(u_material_surface.map_strengths.z, 0.0);
    float occlusion_map_strength = max(u_material_surface.map_strengths.w, 0.0);

    vec4 diffuse_sample = texture(u_diffuse_map, v_tex_coord);
    vec3 normal_sample = texture(u_normal_map, v_tex_coord).xyz;
    float specular_sample = texture(u_specular_map, v_tex_coord).r;
    float shininess_sample = texture(u_shininess_map, v_tex_coord).r;
    vec3 emissive_sample = texture(u_emissive_map, v_tex_coord).rgb;
    float occlusion_sample = texture(u_occlusion_map, v_tex_coord).r;

    vec4 texture_color =
        (v_color * u_color) * (vec4(1.0) + ((diffuse_sample - vec4(1.0)) * diffuse_strength));
    if (texture_color.a < alpha_cutoff)
        discard;
    float surface_alpha = texture_color.a * (1.0 - transparency_amount);

    float specular = clamp(
        specular_base * tbx_apply_map_strength(1.0, specular_sample, specular_map_strength),
        0.0,
        1.0);
    float shininess = clamp(
        shininess_base * tbx_apply_map_strength(1.0, shininess_sample, shininess_map_strength),
        1.0,
        256.0);
    vec3 emissive =
        u_emissive.rgb
        * tbx_apply_map_strength(vec3(1.0), emissive_sample, emissive_map_strength);
    float occlusion = clamp(
        occlusion_base * tbx_apply_map_strength(1.0, occlusion_sample, occlusion_map_strength),
        0.0,
        1.0);

    vec3 tangent_space_normal = (normal_sample * 2.0) - 1.0;
    tangent_space_normal = normalize(
        vec3(
            tangent_space_normal.xy * normal_strength,
            max(tangent_space_normal.z, 0.0001)));
    vec3 normalized_world_normal = normalize(v_world_normal);
    vec3 normalized_world_tangent =
        normalize(v_world_tangent - normalized_world_normal * dot(v_world_tangent, normalized_world_normal));
    vec3 normalized_world_bitangent =
        normalize(cross(normalized_world_normal, normalized_world_tangent)) * v_world_tangent_sign;
    mat3 tangent_to_world = mat3(
        normalized_world_tangent,
        normalized_world_bitangent,
        normalized_world_normal);
    vec3 mapped_world_normal = normalize(tangent_to_world * tangent_space_normal);
    vec3 preview_light_direction = normalize(vec3(0.35, 0.8, 0.45));
    vec3 preview_view_direction = normalize(vec3(0.15, 0.3, 1.0) - v_world_position);
    float preview_n_dot_l = max(dot(mapped_world_normal, preview_light_direction), 0.0);
    vec3 preview_half_vector = normalize(preview_light_direction + preview_view_direction);
    float preview_specular =
        pow(max(dot(mapped_world_normal, preview_half_vector), 0.0), shininess) * specular;
    float hemisphere_factor = clamp((mapped_world_normal.y * 0.5) + 0.5, 0.0, 1.0);
    vec3 hemisphere_ambient =
        mix(vec3(0.05, 0.045, 0.04), vec3(0.17, 0.19, 0.23), hemisphere_factor) * occlusion;
    float preview_fresnel =
        pow(1.0 - max(dot(mapped_world_normal, preview_view_direction), 0.0), 4.0);
    vec3 forward_lit_color =
        (texture_color.rgb * (hemisphere_ambient + vec3(preview_n_dot_l * occlusion)))
        + vec3(preview_specular)
        + vec3(specular * preview_fresnel * 0.08)
        + (emissive * 0.5);
    float depth_preview = 1.0 - pow(clamp(gl_FragCoord.z, 0.0, 1.0), 24.0);

    o_final_color = vec4(forward_lit_color, surface_alpha);
    o_geometry_preview_color = vec4(forward_lit_color, surface_alpha);
    o_albedo = vec4(texture_color.rgb, surface_alpha);
    o_normal = vec4((mapped_world_normal * 0.5) + 0.5, 1.0);
    o_depth_preview = vec4(vec3(depth_preview), 1.0);
    o_emissive = vec4(emissive, exposure);
    o_material = vec4(specular, shininess, occlusion, 1.0);
}
