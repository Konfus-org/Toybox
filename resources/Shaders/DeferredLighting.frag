#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec2 v_tex_coord;

uniform sampler2D u_gbuffer_albedo_spec;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_scene_depth;
uniform sampler2DShadow u_shadow_maps[24];

uniform vec3 u_camera_position = vec3(0.0);
uniform vec3 u_camera_forward = vec3(0.0, 0.0, -1.0);
uniform mat4 u_inverse_view_projection = mat4(1.0);
uniform mat4 u_light_view_projection_matrices[24];
uniform int u_directional_light_count = 0;
uniform int u_point_light_count = 0;
uniform int u_spot_light_count = 0;
uniform int u_shadow_map_count = 0;
uniform float u_cascade_splits[4] = float[4](10.0, 25.0, 60.0, 1000.0);
uniform float u_shadow_softness = 1.75;
uniform float u_exposure = 0.6;

struct DirectionalLight
{
    vec3 direction;
    float intensity;
    vec3 color;
    float ambient;
    int shadow_map_index;
};

struct PointLight
{
    vec3 position;
    float range;
    vec3 color;
    float intensity;
    int shadow_map_index;
};

struct SpotLight
{
    vec3 position;
    float range;
    vec3 direction;
    float inner_cos;
    vec3 color;
    float outer_cos;
    float intensity;
    int shadow_map_index;
};

const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_POINT_LIGHTS = 32;
const int MAX_SPOT_LIGHTS = 16;
const int DIRECTIONAL_SHADOW_CASCADE_COUNT = 2;
const int POINT_SHADOW_FACE_COUNT = 6;
const float SHADOW_CLIP_EPSILON = 0.002;

uniform DirectionalLight u_directional_lights[MAX_DIRECTIONAL_LIGHTS];
uniform PointLight u_point_lights[MAX_POINT_LIGHTS];
uniform SpotLight u_spot_lights[MAX_SPOT_LIGHTS];

float calc_distance_attenuation(float distance_to_light, float range)
{
    if (range <= 0.0)
        return 0.0;

    float normalized = clamp(distance_to_light / range, 0.0, 1.0);
    float attenuation_curve = 1.0 - normalized * normalized;
    return attenuation_curve * attenuation_curve / max(distance_to_light * distance_to_light, 0.01);
}

vec3 reconstruct_world_position(vec2 uv, float depth)
{
    vec4 clip_space = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = u_inverse_view_projection * clip_space;
    float w = world.w;
    if (abs(w) <= 0.000001)
        return vec3(0.0);

    return world.xyz / w;
}

vec3 safe_normalize(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    if (length_squared <= 0.0000001)
        return fallback;

    return value * inversesqrt(length_squared);
}

int select_point_shadow_face_index(vec3 light_to_fragment)
{
    vec3 abs_vector = abs(light_to_fragment);
    if (abs_vector.x >= abs_vector.y && abs_vector.x >= abs_vector.z)
        return light_to_fragment.x >= 0.0 ? 0 : 1;

    if (abs_vector.y >= abs_vector.x && abs_vector.y >= abs_vector.z)
        return light_to_fragment.y >= 0.0 ? 2 : 3;

    return light_to_fragment.z >= 0.0 ? 4 : 5;
}

float sample_shadow_visibility(
    int shadow_map_index,
    vec2 shadow_uv,
    float current_depth,
    float bias,
    vec3 world_position,
    float softness_scale,
    int sample_count)
{
    vec2 texel_size = 1.0 / vec2(textureSize(u_shadow_maps[shadow_map_index], 0));
    const vec2 poisson_disk[12] = vec2[](
        vec2(-0.326, -0.406),
        vec2(-0.840, -0.074),
        vec2(-0.696, 0.457),
        vec2(-0.203, 0.621),
        vec2(0.962, -0.195),
        vec2(0.473, -0.480),
        vec2(0.519, 0.767),
        vec2(0.185, -0.893),
        vec2(0.507, 0.064),
        vec2(0.896, 0.412),
        vec2(-0.322, -0.933),
        vec2(-0.792, -0.598));

    float shadow_visibility = texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv, current_depth - bias));
    if (shadow_visibility >= 0.999)
        return 1.0;

    float radius_in_texels = clamp(u_shadow_softness * softness_scale, 0.0, 4.0);
    if (radius_in_texels <= 0.001)
        return shadow_visibility;

    float edge_distance = min(
        min(shadow_uv.x, 1.0 - shadow_uv.x),
        min(shadow_uv.y, 1.0 - shadow_uv.y));
    float max_texel_size = max(texel_size.x, texel_size.y);
    float available_radius_in_uv = max(edge_distance - SHADOW_CLIP_EPSILON, 0.0);
    float available_radius_in_texels = available_radius_in_uv / max(max_texel_size, 0.000001);
    radius_in_texels = min(radius_in_texels, available_radius_in_texels);
    if (radius_in_texels <= 0.001)
        return shadow_visibility;

    float hash_value = fract(sin(dot(world_position.xz, vec2(12.9898, 78.233))) * 43758.5453);
    float rotation = hash_value * 6.2831853;
    mat2 rotation_matrix = mat2(cos(rotation), -sin(rotation), sin(rotation), cos(rotation));

    const int max_sample_count = 8;
    int clamped_sample_count = clamp(sample_count, 1, max_sample_count);
    float visibility = 0.0;
    for (int index = 0; index < max_sample_count; ++index)
    {
        if (index >= clamped_sample_count)
            break;

        vec2 offset = rotation_matrix * poisson_disk[index] * texel_size * radius_in_texels;
        visibility += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + offset, current_depth - bias));
    }

    return visibility / float(clamped_sample_count);
}

float sample_projected_shadow_visibility(
    int shadow_map_index,
    vec4 light_space_position,
    float bias,
    vec3 world_position,
    float softness_scale,
    int sample_count)
{
    if (shadow_map_index < 0 || shadow_map_index >= u_shadow_map_count)
        return 1.0;

    vec3 projected = light_space_position.xyz / max(light_space_position.w, 0.0001);
    vec2 shadow_uv = projected.xy * 0.5 + 0.5;
    float current_depth = projected.z * 0.5 + 0.5;
    if (shadow_uv.x < (-SHADOW_CLIP_EPSILON) || shadow_uv.x > (1.0 + SHADOW_CLIP_EPSILON)
        || shadow_uv.y < (-SHADOW_CLIP_EPSILON) || shadow_uv.y > (1.0 + SHADOW_CLIP_EPSILON)
        || current_depth < (-SHADOW_CLIP_EPSILON) || current_depth > (1.0 + SHADOW_CLIP_EPSILON))
        return 1.0;

    shadow_uv = clamp(shadow_uv, vec2(0.0), vec2(1.0));
    current_depth = clamp(current_depth, 0.0, 1.0);
    return sample_shadow_visibility(
        shadow_map_index,
        shadow_uv,
        current_depth,
        bias,
        world_position,
        softness_scale,
        sample_count);
}

bool try_project_shadow_coordinates(
    vec4 light_space_position,
    out vec2 out_shadow_uv,
    out float out_current_depth)
{
    float safe_w = light_space_position.w;
    if (abs(safe_w) <= 0.0001)
        return false;

    vec3 projected = light_space_position.xyz / safe_w;
    vec2 shadow_uv = projected.xy * 0.5 + 0.5;
    float current_depth = projected.z * 0.5 + 0.5;
    if (shadow_uv.x < (-SHADOW_CLIP_EPSILON) || shadow_uv.x > (1.0 + SHADOW_CLIP_EPSILON)
        || shadow_uv.y < (-SHADOW_CLIP_EPSILON) || shadow_uv.y > (1.0 + SHADOW_CLIP_EPSILON)
        || current_depth < (-SHADOW_CLIP_EPSILON) || current_depth > (1.0 + SHADOW_CLIP_EPSILON))
        return false;

    out_shadow_uv = clamp(shadow_uv, vec2(0.0), vec2(1.0));
    out_current_depth = clamp(current_depth, 0.0, 1.0);
    return true;
}

float calculate_directional_shadow_bias(int shadow_map_index, float ndotl, float cascade_scale)
{
    vec2 texel_size = 1.0 / vec2(textureSize(u_shadow_maps[shadow_map_index], 0));
    float max_texel_size = max(texel_size.x, texel_size.y);
    float slope_factor = 1.0 - clamp(ndotl, 0.0, 1.0);
    float slope_bias = max(0.00003 * cascade_scale * slope_factor, 0.00001);
    float texel_bias = max_texel_size * (1.5 * cascade_scale);
    return slope_bias + texel_bias;
}

vec3 evaluate_directional_light(
    DirectionalLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 light_direction = safe_normalize(light.direction, vec3(0.0, 0.0, -1.0));
    float diffuse_term = max(dot(normal, light_direction), 0.0);
    vec3 half_direction = safe_normalize(light_direction + view_direction, normal);
    float specular_term = pow(max(dot(normal, half_direction), 0.0), shininess) * specular_strength;

    vec3 diffuse = albedo * light.color * light.intensity * diffuse_term * 0.35;
    vec3 specular = light.color * light.intensity * specular_term;
    vec3 ambient = albedo * light.color * max(light.ambient, 0.0);

    float shadow_visibility = 1.0;
    int base_shadow_map_index = light.shadow_map_index;
    if (base_shadow_map_index >= 0)
    {
        float ndotl = clamp(dot(normal, light_direction), 0.0, 1.0);
        vec3 shadow_position = world_position + (normal * 0.0008);

        if (base_shadow_map_index + (DIRECTIONAL_SHADOW_CASCADE_COUNT - 1) < u_shadow_map_count)
        {
            float view_depth = max(dot(world_position - u_camera_position, u_camera_forward), 0.0);
            float split = max(u_cascade_splits[0], 0.001);
            float blend_range = max(split * 0.08, 1.25);
            float far_cascade_weight =
                smoothstep(split - blend_range, split + blend_range, view_depth);

            int near_shadow_map_index = base_shadow_map_index;
            int far_shadow_map_index = base_shadow_map_index + 1;
            float near_bias = calculate_directional_shadow_bias(near_shadow_map_index, ndotl, 1.0);
            float far_bias = calculate_directional_shadow_bias(far_shadow_map_index, ndotl, 1.25);
            vec4 near_light_space_position =
                u_light_view_projection_matrices[near_shadow_map_index] * vec4(shadow_position, 1.0);
            vec4 far_light_space_position =
                u_light_view_projection_matrices[far_shadow_map_index] * vec4(shadow_position, 1.0);

            vec2 near_shadow_uv = vec2(0.0);
            vec2 far_shadow_uv = vec2(0.0);
            float near_current_depth = 0.0;
            float far_current_depth = 0.0;
            bool has_near_coverage = try_project_shadow_coordinates(
                near_light_space_position,
                near_shadow_uv,
                near_current_depth);
            bool has_far_coverage = try_project_shadow_coordinates(
                far_light_space_position,
                far_shadow_uv,
                far_current_depth);

            float near_visibility = 1.0;
            if (has_near_coverage)
            {
                near_visibility = sample_shadow_visibility(
                    near_shadow_map_index,
                    near_shadow_uv,
                    near_current_depth,
                    near_bias,
                    world_position,
                    1.0,
                    8);
            }

            float far_visibility = 1.0;
            if (has_far_coverage)
            {
                far_visibility = sample_shadow_visibility(
                    far_shadow_map_index,
                    far_shadow_uv,
                    far_current_depth,
                    far_bias,
                    world_position,
                    1.0,
                    8);
            }

            float blended_visibility = 1.0;
            if (has_near_coverage && has_far_coverage)
                blended_visibility = mix(near_visibility, far_visibility, far_cascade_weight);
            else if (has_near_coverage)
                blended_visibility = near_visibility;
            else if (has_far_coverage)
                blended_visibility = far_visibility;
            shadow_visibility = mix(0.15, 1.0, blended_visibility);
        }
        else
        {
            float base_bias = calculate_directional_shadow_bias(base_shadow_map_index, ndotl, 1.0);
            float pcf_visibility = sample_projected_shadow_visibility(
                base_shadow_map_index,
                u_light_view_projection_matrices[base_shadow_map_index]
                    * vec4(shadow_position, 1.0),
                base_bias,
                world_position,
                1.0,
                8);
            shadow_visibility = mix(0.15, 1.0, pcf_visibility);
        }
    }

    return ambient + ((diffuse + specular) * shadow_visibility);
}

vec3 evaluate_point_light(
    PointLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 to_light = light.position - world_position;
    float distance_to_light = length(to_light);
    if (distance_to_light >= light.range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    float attenuation = calc_distance_attenuation(distance_to_light, light.range);
    float diffuse_term = max(dot(normal, light_direction), 0.0);
    vec3 half_direction = safe_normalize(light_direction + view_direction, normal);
    float specular_term = pow(max(dot(normal, half_direction), 0.0), shininess) * specular_strength;

    float shadow_visibility = 1.0;
    int base_shadow_map_index = light.shadow_map_index;
    if (base_shadow_map_index >= 0
        && base_shadow_map_index + (POINT_SHADOW_FACE_COUNT - 1) < u_shadow_map_count)
    {
        vec3 light_to_fragment = world_position - light.position;
        int face_index = select_point_shadow_face_index(light_to_fragment);
        int shadow_map_index = base_shadow_map_index + face_index;
        float ndotl = clamp(dot(normal, light_direction), 0.0, 1.0);
        float bias = max(0.0004 * (1.0 - ndotl), 0.0001);
        vec3 shadow_position = world_position + (normal * 0.005);
        float pcf_visibility = sample_projected_shadow_visibility(
            shadow_map_index,
            u_light_view_projection_matrices[shadow_map_index] * vec4(shadow_position, 1.0),
            bias,
            world_position,
            0.8,
            4);
        shadow_visibility = mix(0.2, 1.0, pcf_visibility);
    }

    vec3 diffuse = albedo * light.color * light.intensity * diffuse_term * 0.35;
    vec3 specular = light.color * light.intensity * specular_term;
    return (diffuse + specular) * attenuation * shadow_visibility;
}

vec3 evaluate_spot_light(
    SpotLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 to_light = light.position - world_position;
    float distance_to_light = length(to_light);
    if (distance_to_light >= light.range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    vec3 spot_axis = safe_normalize(-light.direction, vec3(0.0, 0.0, -1.0));
    float spot_cos = dot(light_direction, spot_axis);
    if (spot_cos <= light.outer_cos)
        return vec3(0.0);

    float cone = clamp(
        (spot_cos - light.outer_cos) / max(light.inner_cos - light.outer_cos, 0.0001),
        0.0,
        1.0);
    float attenuation = calc_distance_attenuation(distance_to_light, light.range) * cone;
    float diffuse_term = max(dot(normal, light_direction), 0.0);
    vec3 half_direction = safe_normalize(light_direction + view_direction, normal);
    float specular_term = pow(max(dot(normal, half_direction), 0.0), shininess) * specular_strength;

    float shadow_visibility = 1.0;
    int shadow_map_index = light.shadow_map_index;
    if (shadow_map_index >= 0)
    {
        float ndotl = clamp(dot(normal, light_direction), 0.0, 1.0);
        float bias = max(0.0002 * (1.0 - ndotl), 0.00004);
        vec3 shadow_position = world_position + (normal * 0.003);
        float pcf_visibility = sample_projected_shadow_visibility(
            shadow_map_index,
            u_light_view_projection_matrices[shadow_map_index] * vec4(shadow_position, 1.0),
            bias,
            world_position,
            0.9,
            6);
        shadow_visibility = mix(0.2, 1.0, pcf_visibility);
    }

    vec3 diffuse = albedo * light.color * light.intensity * diffuse_term * 0.35;
    vec3 specular = light.color * light.intensity * specular_term;
    return (diffuse + specular) * attenuation * shadow_visibility;
}

void main()
{
    vec4 albedo_spec = texture(u_gbuffer_albedo_spec, v_tex_coord);
    vec4 normal_data = texture(u_gbuffer_normal, v_tex_coord);
    vec4 material_data = texture(u_gbuffer_material, v_tex_coord);
    float depth = texture(u_scene_depth, v_tex_coord).r;

    if (depth >= 0.999999)
    {
        o_color = vec4(albedo_spec.rgb, 1.0);
        return;
    }

    vec3 world_position = reconstruct_world_position(v_tex_coord, depth);
    vec3 normal = safe_normalize(normal_data.xyz * 2.0 - 1.0, vec3(0.0, 1.0, 0.0));
    vec3 view_direction = safe_normalize(u_camera_position - world_position, vec3(0.0, 0.0, 1.0));
    vec3 albedo = albedo_spec.rgb;
    bool is_unlit = normal_data.a > 0.5;
    float specular_strength = clamp(albedo_spec.a, 0.0, 1.0);
    float shininess = max(material_data.a * 128.0, 1.0);
    vec3 emissive = material_data.rgb;

    if (is_unlit)
    {
        vec3 unlit_color = tbx_linear_to_srgb(albedo + emissive);
        o_color = vec4(unlit_color, 1.0);
        return;
    }

    vec3 lighting = emissive;
    for (int index = 0; index < min(u_directional_light_count, MAX_DIRECTIONAL_LIGHTS); ++index)
    {
        lighting += evaluate_directional_light(
            u_directional_lights[index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    for (int index = 0; index < min(u_point_light_count, MAX_POINT_LIGHTS); ++index)
    {
        lighting += evaluate_point_light(
            u_point_lights[index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    for (int index = 0; index < min(u_spot_light_count, MAX_SPOT_LIGHTS); ++index)
    {
        lighting += evaluate_spot_light(
            u_spot_lights[index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }

    vec3 bounded_lighting = clamp(lighting, vec3(0.0), vec3(32.0));
    vec3 mapped = tbx_tonemap_aces(bounded_lighting * max(u_exposure, 0.0));
    mapped = tbx_linear_to_srgb(mapped);
    o_color = vec4(mapped, 1.0);
}
