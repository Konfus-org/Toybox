#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

layout(location = 0) in flat uint v_packed_light_index;

uniform sampler2D u_gbuffer_albedo_spec;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_scene_depth;

uniform vec3 u_camera_position = vec3(0.0);
uniform mat4 u_inverse_view_projection = mat4(1.0);
uniform float u_exposure = 0.6;

struct PackedLight
{
    vec4 position_range;
    vec4 direction_inner_cos;
    vec4 color_intensity;
    vec4 area_outer_ambient;
    ivec4 metadata;
};

layout(std430, binding = 0) readonly buffer PackedLightBuffer
{
    PackedLight packed_lights[];
};

const int LIGHT_TYPE_POINT = 1;
const int LIGHT_TYPE_SPOT = 2;
const int LIGHT_TYPE_AREA = 3;

vec3 safe_normalize(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    if (length_squared <= 1e-10)
        return fallback;
    return value * inversesqrt(length_squared);
}

vec3 reconstruct_world_position(vec2 uv, float depth)
{
    vec4 clip_space = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = u_inverse_view_projection * clip_space;
    if (abs(world.w) <= 1e-7)
        return vec3(0.0);
    return world.xyz / world.w;
}

float calc_distance_attenuation(float distance_to_light, float range)
{
    if (range <= 0.0)
        return 0.0;
    float normalized = clamp(distance_to_light / range, 0.0, 1.0);
    float attenuation_curve = 1.0 - normalized * normalized;
    return attenuation_curve * attenuation_curve / max(distance_to_light * distance_to_light, 0.01);
}

void build_light_basis(vec3 axis, out vec3 tangent, out vec3 bitangent)
{
    vec3 fallback_up = abs(axis.y) > 0.95 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    tangent = safe_normalize(cross(fallback_up, axis), vec3(1.0, 0.0, 0.0));
    bitangent = safe_normalize(cross(axis, tangent), vec3(0.0, 0.0, 1.0));
}

vec3 evaluate_lighting_brdf(
    vec3 normal,
    vec3 view_direction,
    vec3 light_direction,
    vec3 albedo,
    vec3 light_color,
    float light_intensity,
    float specular_strength,
    float shininess)
{
    float diffuse_term = max(dot(normal, light_direction), 0.0);
    vec3 half_direction = safe_normalize(light_direction + view_direction, normal);
    float specular_term = pow(max(dot(normal, half_direction), 0.0), shininess) * specular_strength;
    vec3 diffuse = albedo * light_color * light_intensity * diffuse_term * 0.35;
    vec3 specular = light_color * light_intensity * specular_term;
    return diffuse + specular;
}

vec3 evaluate_point_light(
    PackedLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 to_light = light.position_range.xyz - world_position;
    float distance_to_light = length(to_light);
    float range = light.position_range.w;
    if (distance_to_light >= range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    float attenuation = calc_distance_attenuation(distance_to_light, range);
    return evaluate_lighting_brdf(
               normal,
               view_direction,
               light_direction,
               albedo,
               light.color_intensity.xyz,
               light.color_intensity.w,
               specular_strength,
               shininess)
        * attenuation;
}

vec3 evaluate_spot_light(
    PackedLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 to_light = light.position_range.xyz - world_position;
    float distance_to_light = length(to_light);
    float range = light.position_range.w;
    if (distance_to_light >= range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    vec3 spot_axis = safe_normalize(-light.direction_inner_cos.xyz, vec3(0.0, 0.0, -1.0));
    float spot_cos = dot(light_direction, spot_axis);
    float inner_cos = light.direction_inner_cos.w;
    float outer_cos = light.area_outer_ambient.z;
    if (spot_cos <= outer_cos)
        return vec3(0.0);

    float cone = clamp((spot_cos - outer_cos) / max(inner_cos - outer_cos, 0.0001), 0.0, 1.0);
    float attenuation = calc_distance_attenuation(distance_to_light, range) * cone;
    return evaluate_lighting_brdf(
               normal,
               view_direction,
               light_direction,
               albedo,
               light.color_intensity.xyz,
               light.color_intensity.w,
               specular_strength,
               shininess)
        * attenuation;
}

vec3 evaluate_area_light(
    PackedLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    float range = light.position_range.w;
    if (range <= 0.0)
        return vec3(0.0);

    vec3 emission_axis = safe_normalize(light.direction_inner_cos.xyz, vec3(0.0, -1.0, 0.0));
    vec3 tangent = vec3(1.0, 0.0, 0.0);
    vec3 bitangent = vec3(0.0, 0.0, 1.0);
    build_light_basis(emission_axis, tangent, bitangent);

    vec2 half_size = max(light.area_outer_ambient.xy * 0.5, vec2(0.001));
    vec3 local_offset = world_position - light.position_range.xyz;
    float axis_distance = dot(local_offset, emission_axis);
    if (axis_distance < 0.0 || axis_distance > range)
        return vec3(0.0);

    float tangent_distance = dot(local_offset, tangent);
    float bitangent_distance = dot(local_offset, bitangent);
    float normalized_tangent_distance = abs(tangent_distance) / half_size.x;
    float normalized_bitangent_distance = abs(bitangent_distance) / half_size.y;
    if (normalized_tangent_distance >= 1.0 || normalized_bitangent_distance >= 1.0)
        return vec3(0.0);

    vec3 closest_point = light.position_range.xyz + (tangent * tangent_distance)
        + (bitangent * bitangent_distance);
    vec3 to_light = closest_point - world_position;
    float distance_to_light = length(to_light);
    if (distance_to_light >= range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    vec3 light_to_fragment_direction = -light_direction;
    float front_cos = dot(light_to_fragment_direction, emission_axis);
    if (front_cos <= 0.0)
        return vec3(0.0);

    float edge_falloff = 1.0 - max(normalized_tangent_distance, normalized_bitangent_distance);
    edge_falloff = edge_falloff * edge_falloff * (3.0 - (2.0 * edge_falloff));
    float depth_falloff = 1.0 - clamp(axis_distance / range, 0.0, 1.0);
    float attenuation = calc_distance_attenuation(distance_to_light, range);
    attenuation *= front_cos * edge_falloff * depth_falloff;
    return evaluate_lighting_brdf(
               normal,
               view_direction,
               light_direction,
               albedo,
               light.color_intensity.xyz,
               light.color_intensity.w,
               specular_strength,
               shininess)
        * attenuation;
}

void main()
{
    vec2 depth_size = vec2(max(textureSize(u_scene_depth, 0), ivec2(1)));
    vec2 uv = gl_FragCoord.xy / depth_size;
    vec4 albedo_spec = texture(u_gbuffer_albedo_spec, uv);
    vec4 normal_data = texture(u_gbuffer_normal, uv);
    vec4 material_data = texture(u_gbuffer_material, uv);
    float depth = texture(u_scene_depth, uv).r;
    if (depth >= 0.999999 || normal_data.a > 0.5)
    {
        o_color = vec4(0.0);
        return;
    }

    PackedLight light = packed_lights[v_packed_light_index];
    int light_type = light.metadata.x;
    if (light_type != LIGHT_TYPE_POINT && light_type != LIGHT_TYPE_SPOT && light_type != LIGHT_TYPE_AREA)
    {
        o_color = vec4(0.0);
        return;
    }

    vec3 world_position = reconstruct_world_position(uv, depth);
    vec3 normal = safe_normalize(normal_data.xyz * 2.0 - 1.0, vec3(0.0, 1.0, 0.0));
    vec3 view_direction = safe_normalize(u_camera_position - world_position, vec3(0.0, 0.0, 1.0));
    vec3 albedo = albedo_spec.rgb;
    float specular_strength = clamp(albedo_spec.a, 0.0, 1.0);
    float shininess = max(material_data.a * 128.0, 1.0);

    vec3 local_contribution = vec3(0.0);
    if (light_type == LIGHT_TYPE_POINT)
    {
        local_contribution = evaluate_point_light(
            light,
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    else if (light_type == LIGHT_TYPE_SPOT)
    {
        local_contribution = evaluate_spot_light(
            light,
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    else
    {
        local_contribution = evaluate_area_light(
            light,
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }

    vec3 mapped = tbx_tonemap_aces(local_contribution * max(u_exposure, 0.0));
    mapped = tbx_linear_to_srgb(mapped);
    o_color = vec4(mapped, 1.0);
}
