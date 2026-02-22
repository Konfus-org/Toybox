#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec2 v_tex_coord;

uniform sampler2D u_gbuffer_albedo_spec;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_scene_depth;
uniform sampler2D u_shadow_maps[24];

uniform vec3 u_camera_position = vec3(0.0);
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
    float softness_scale)
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

    float reference_depth = texture(u_shadow_maps[shadow_map_index], shadow_uv).r;
    if (current_depth - bias <= reference_depth)
        return 1.0;

    float radius_in_texels = clamp(u_shadow_softness * softness_scale, 0.0, 4.0);
    if (radius_in_texels <= 0.001)
        return 0.0;

    float hash_value = fract(sin(dot(world_position.xz, vec2(12.9898, 78.233))) * 43758.5453);
    float rotation = hash_value * 6.2831853;
    mat2 rotation_matrix = mat2(cos(rotation), -sin(rotation), sin(rotation), cos(rotation));

    float visibility = 0.0;
    const int sample_count = 8;
    for (int index = 0; index < sample_count; ++index)
    {
        vec2 offset = rotation_matrix * poisson_disk[index] * texel_size * radius_in_texels;
        float shadow_depth = texture(u_shadow_maps[shadow_map_index], shadow_uv + offset).r;
        visibility += current_depth - bias <= shadow_depth ? 1.0 : 0.0;
    }

    return visibility / float(sample_count);
}

float sample_projected_shadow_visibility(
    int shadow_map_index,
    vec4 light_space_position,
    float bias,
    vec3 world_position,
    float softness_scale)
{
    if (shadow_map_index < 0 || shadow_map_index >= u_shadow_map_count)
        return 1.0;

    vec3 projected = light_space_position.xyz / max(light_space_position.w, 0.0001);
    float current_depth = projected.z * 0.5 + 0.5;
    if (projected.x < -1.0 || projected.x > 1.0 || projected.y < -1.0 || projected.y > 1.0
        || current_depth < 0.0 || current_depth > 1.0)
        return 1.0;

    vec2 shadow_uv = projected.xy * 0.5 + 0.5;
    return sample_shadow_visibility(
        shadow_map_index,
        shadow_uv,
        current_depth,
        bias,
        world_position,
        softness_scale);
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
        int selected_shadow_map_index = base_shadow_map_index;
        float softness_scale = 1.0;
        if (base_shadow_map_index + (DIRECTIONAL_SHADOW_CASCADE_COUNT - 1) < u_shadow_map_count)
        {
            float distance_to_camera = distance(world_position, u_camera_position);
            int cascade_index = distance_to_camera <= u_cascade_splits[0] ? 0 : 1;
            selected_shadow_map_index = base_shadow_map_index + cascade_index;
            softness_scale = cascade_index == 0 ? 0.8 : 1.0;
        }

        float ndotl = clamp(dot(normal, light_direction), 0.0, 1.0);
        float bias = max(0.0004 * (1.0 - ndotl), 0.00005);
        float pcf_visibility = sample_projected_shadow_visibility(
            selected_shadow_map_index,
            u_light_view_projection_matrices[selected_shadow_map_index] * vec4(world_position, 1.0),
            bias,
            world_position,
            softness_scale);
        shadow_visibility = mix(0.15, 1.0, pcf_visibility);
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
    if (base_shadow_map_index >= 0 && base_shadow_map_index + (POINT_SHADOW_FACE_COUNT - 1) < u_shadow_map_count)
    {
        vec3 light_to_fragment = world_position - light.position;
        int face_index = select_point_shadow_face_index(light_to_fragment);
        int shadow_map_index = base_shadow_map_index + face_index;
        float ndotl = clamp(dot(normal, light_direction), 0.0, 1.0);
        float bias = max(0.0012 * (1.0 - ndotl), 0.00025);
        float pcf_visibility = sample_projected_shadow_visibility(
            shadow_map_index,
            u_light_view_projection_matrices[shadow_map_index] * vec4(world_position, 1.0),
            bias,
            world_position,
            0.9);
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
        float bias = max(0.0005 * (1.0 - ndotl), 0.00008);
        float pcf_visibility = sample_projected_shadow_visibility(
            shadow_map_index,
            u_light_view_projection_matrices[shadow_map_index] * vec4(world_position, 1.0),
            bias,
            world_position,
            1.0);
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
