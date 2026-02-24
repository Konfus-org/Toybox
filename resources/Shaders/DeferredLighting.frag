#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec2 v_tex_coord;

uniform sampler2D u_gbuffer_albedo_spec;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_scene_depth;

uniform vec3 u_camera_position = vec3(0.0);
uniform mat4 u_view_projection = mat4(1.0);
uniform mat4 u_inverse_view_projection = mat4(1.0);
uniform int u_directional_light_count = 0;
uniform int u_point_light_count = 0;
uniform int u_spot_light_count = 0;
uniform float u_exposure = 0.6;
uniform float u_sdf_shadow_step_scale = 0.75;
uniform int u_sdf_shadow_max_steps = 24;
uniform float u_sdf_shadow_max_distance = 96.0;
uniform float u_sdf_surface_epsilon = 0.025;
uniform float u_sdf_ao_strength = 0.65;

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

float evaluate_screen_space_signed_distance(vec3 world_sample)
{
    vec4 clip_sample = u_view_projection * vec4(world_sample, 1.0);
    if (clip_sample.w <= 0.0001)
        return u_sdf_shadow_max_distance;

    vec3 projected = clip_sample.xyz / clip_sample.w;
    if (projected.x < -1.0 || projected.x > 1.0 || projected.y < -1.0 || projected.y > 1.0)
        return u_sdf_shadow_max_distance;

    vec2 sample_uv = projected.xy * 0.5 + 0.5;
    float scene_depth = texture(u_scene_depth, sample_uv).r;
    if (scene_depth >= 0.999999)
        return u_sdf_shadow_max_distance;

    vec3 scene_surface = reconstruct_world_position(sample_uv, scene_depth);
    float euclidean_distance = length(world_sample - scene_surface);
    float sample_to_camera = length(world_sample - u_camera_position);
    float surface_to_camera = length(scene_surface - u_camera_position);
    float sign = sample_to_camera > surface_to_camera ? -1.0 : 1.0;
    return euclidean_distance * sign;
}

float trace_sdf_shadow(vec3 origin, vec3 direction, float max_distance)
{
    float march_distance = max(u_sdf_surface_epsilon * 2.0, 0.01);
    float penumbra = 1.0;
    int max_steps = clamp(u_sdf_shadow_max_steps, 4, 64);

    for (int step_index = 0; step_index < 64; ++step_index)
    {
        if (step_index >= max_steps || march_distance >= max_distance)
            break;

        vec3 sample_position = origin + direction * march_distance;
        float signed_distance = evaluate_screen_space_signed_distance(sample_position);
        if (signed_distance <= u_sdf_surface_epsilon)
            return 0.0;

        penumbra = min(penumbra, 12.0 * signed_distance / max(march_distance, 0.001));
        float step_size = clamp(signed_distance * u_sdf_shadow_step_scale, 0.04, 1.25);
        march_distance += step_size;
    }

    return clamp(penumbra, 0.0, 1.0);
}

float evaluate_sdf_ambient_occlusion(vec3 world_position, vec3 normal)
{
    float occlusion = 0.0;
    const int sample_count = 5;
    for (int sample_index = 1; sample_index <= sample_count; ++sample_index)
    {
        float sample_distance = float(sample_index) * 0.35;
        vec3 sample_position = world_position + normal * sample_distance;
        float signed_distance = evaluate_screen_space_signed_distance(sample_position);
        float normalized_distance = clamp(signed_distance / sample_distance, -1.0, 1.0);
        occlusion += clamp(1.0 - normalized_distance, 0.0, 1.0);
    }

    float averaged_occlusion = occlusion / float(sample_count);
    return clamp(1.0 - averaged_occlusion * u_sdf_ao_strength, 0.15, 1.0);
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

    float shadow_visibility = trace_sdf_shadow(world_position + normal * 0.05, light_direction, u_sdf_shadow_max_distance);
    vec3 diffuse = albedo * light.color * light.intensity * diffuse_term * 0.35;
    vec3 specular = light.color * light.intensity * specular_term;
    vec3 ambient = albedo * light.color * max(light.ambient, 0.0);
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

    float shadow_visibility = trace_sdf_shadow(world_position + normal * 0.05, light_direction, distance_to_light);
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

    float shadow_visibility = trace_sdf_shadow(world_position + normal * 0.05, light_direction, distance_to_light);
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

    lighting *= evaluate_sdf_ambient_occlusion(world_position, normal);
    vec3 bounded_lighting = clamp(lighting, vec3(0.0), vec3(32.0));
    vec3 mapped = tbx_tonemap_aces(bounded_lighting * max(u_exposure, 0.0));
    mapped = tbx_linear_to_srgb(mapped);
    o_color = vec4(mapped, 1.0);
}
