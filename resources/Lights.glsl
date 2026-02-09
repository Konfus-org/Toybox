#pragma once

const int TBX_MAX_POINT_LIGHTS = 16;
const int TBX_MAX_AREA_LIGHTS = 8;
const int TBX_MAX_SPOT_LIGHTS = 12;
const int TBX_MAX_DIRECTIONAL_LIGHTS = 5;

struct TbxPointLight
{
    vec3 position;
    vec3 color;
    float intensity;
    float range;
};

struct TbxAreaLight
{
    vec3 position;
    vec3 color;
    float intensity;
    float range;
    vec2 area_size;
};

struct TbxSpotLight
{
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float inner_cos;
    float outer_cos;
};

struct TbxDirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
};

uniform int u_point_light_count;
uniform TbxPointLight u_point_lights[TBX_MAX_POINT_LIGHTS];

uniform int u_area_light_count;
uniform TbxAreaLight u_area_lights[TBX_MAX_AREA_LIGHTS];

uniform int u_spot_light_count;
uniform TbxSpotLight u_spot_lights[TBX_MAX_SPOT_LIGHTS];

uniform int u_directional_light_count;
uniform TbxDirectionalLight u_directional_lights[TBX_MAX_DIRECTIONAL_LIGHTS];

uniform vec3 u_ambient_light = vec3(0.03, 0.03, 0.03);

float tbx_saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 tbx_normalize_light_color(vec3 color)
{
    // Normalize so intensity has comparable effect across hues while leaving white unchanged.
    float energy = dot(max(color, vec3(0.0)), vec3(1.0));
    if (energy <= 0.0001)
        return vec3(0.0);
    return color * (3.0 / energy);
}

float tbx_distribution_ggx(float n_dot_h, float roughness)
{
    float a = max(roughness, 0.02);
    float a2 = a * a;
    float denom = (n_dot_h * n_dot_h) * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denom * denom, 1e-6);
}

float tbx_geometry_schlick_ggx(float n_dot_v, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return n_dot_v / max(n_dot_v * (1.0 - k) + k, 1e-6);
}

float tbx_geometry_smith(float n_dot_v, float n_dot_l, float roughness)
{
    float ggx_v = tbx_geometry_schlick_ggx(n_dot_v, roughness);
    float ggx_l = tbx_geometry_schlick_ggx(n_dot_l, roughness);
    return ggx_v * ggx_l;
}

vec3 tbx_fresnel_schlick(float cos_theta, vec3 f0)
{
    float one_minus = 1.0 - cos_theta;
    float one_minus2 = one_minus * one_minus;
    float one_minus5 = one_minus2 * one_minus2 * one_minus;
    return f0 + (1.0 - f0) * one_minus5;
}

float tbx_compute_distance_attenuation(vec3 light_pos, vec3 world_pos, float range, out vec3 l)
{
    vec3 to_light = light_pos - world_pos;
    float distance2 = max(dot(to_light, to_light), 1e-6);
    float inv_distance = inversesqrt(distance2);
    float distance = distance2 * inv_distance;
    l = to_light * inv_distance;

    float adjusted_range = max(range, 0.001);
    float normalized = distance / adjusted_range;
    float range_falloff = tbx_saturate(1.0 - normalized * normalized * normalized * normalized);
    range_falloff *= range_falloff;

    // Inverse-square attenuation with a smooth range cutoff.
    float inv_distance2 = inv_distance * inv_distance;
    return inv_distance2 * range_falloff;
}

float tbx_compute_spot_factor(
    vec3 light_pos,
    vec3 light_dir,
    vec3 world_pos,
    float inner_cos,
    float outer_cos)
{
    vec3 to_surface = normalize(world_pos - light_pos);
    float spot_cos = dot(normalize(light_dir), to_surface);
    return smoothstep(outer_cos, inner_cos, spot_cos);
}

float tbx_compute_area_factor(vec2 area_size)
{
    float average_extent = 0.5 * (area_size.x + area_size.y);
    return max(1.0, average_extent);
}

vec3 tbx_evaluate_pbr_brdf(
    vec3 n,
    vec3 v,
    vec3 l,
    vec3 albedo,
    float metallic,
    float roughness,
    vec3 f0,
    float n_dot_v)
{
    float n_dot_l = tbx_saturate(dot(n, l));
    if (n_dot_l <= 0.0)
        return vec3(0.0);

    vec3 h = normalize(v + l);
    float n_dot_h = tbx_saturate(dot(n, h));
    float v_dot_h = tbx_saturate(dot(v, h));

    float d = tbx_distribution_ggx(n_dot_h, roughness);
    float g = tbx_geometry_smith(n_dot_v, n_dot_l, roughness);
    vec3 f = tbx_fresnel_schlick(v_dot_h, f0);

    vec3 numerator = d * g * f;
    float denominator = max(4.0 * n_dot_v * n_dot_l, 1e-6);
    vec3 specular = numerator / denominator;

    vec3 k_s = f;
    vec3 k_d = (vec3(1.0) - k_s) * (1.0 - metallic);
    vec3 diffuse = (k_d * albedo) * (1.0 / 3.14159265);

    return (diffuse + specular) * n_dot_l;
}

vec3 tbx_compute_pbr_lighting(
    vec3 world_pos,
    vec3 normal,
    vec3 view_dir,
    vec3 albedo,
    float metallic,
    float roughness)
{
    vec3 n = normalize(normal);
    vec3 v = normalize(view_dir);

    float n_dot_v = tbx_saturate(dot(n, v));
    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    vec3 direct = vec3(0.0);

    int point_count = clamp(u_point_light_count, 0, TBX_MAX_POINT_LIGHTS);
    for (int i = 0; i < point_count; ++i)
    {
        TbxPointLight light = u_point_lights[i];
        float intensity = max(light.intensity, 0.0);
        if (intensity <= 0.0)
            continue;

        vec3 light_color = tbx_normalize_light_color(light.color);
        if (dot(light_color, light_color) <= 0.0)
            continue;

        vec3 l = vec3(0.0, 1.0, 0.0);
        float attenuation = tbx_compute_distance_attenuation(light.position, world_pos, light.range, l);

        vec3 brdf = tbx_evaluate_pbr_brdf(n, v, l, albedo, metallic, roughness, f0, n_dot_v);
        if (dot(brdf, brdf) <= 0.0)
            continue;

        vec3 radiance = light_color * intensity * attenuation;
        direct += brdf * radiance;
    }

    int spot_count = clamp(u_spot_light_count, 0, TBX_MAX_SPOT_LIGHTS);
    for (int i = 0; i < spot_count; ++i)
    {
        TbxSpotLight light = u_spot_lights[i];
        float intensity = max(light.intensity, 0.0);
        if (intensity <= 0.0)
            continue;

        vec3 light_color = tbx_normalize_light_color(light.color);
        if (dot(light_color, light_color) <= 0.0)
            continue;

        vec3 l = vec3(0.0, 1.0, 0.0);
        float attenuation = tbx_compute_distance_attenuation(light.position, world_pos, light.range, l);
        float spot_factor =
            tbx_compute_spot_factor(light.position, light.direction, world_pos, light.inner_cos, light.outer_cos);

        vec3 brdf = tbx_evaluate_pbr_brdf(n, v, l, albedo, metallic, roughness, f0, n_dot_v);
        if (dot(brdf, brdf) <= 0.0)
            continue;

        vec3 radiance = light_color * intensity * attenuation * spot_factor;
        direct += brdf * radiance;
    }

    int area_count = clamp(u_area_light_count, 0, TBX_MAX_AREA_LIGHTS);
    for (int i = 0; i < area_count; ++i)
    {
        TbxAreaLight light = u_area_lights[i];
        float intensity = max(light.intensity, 0.0);
        if (intensity <= 0.0)
            continue;

        vec3 light_color = tbx_normalize_light_color(light.color);
        if (dot(light_color, light_color) <= 0.0)
            continue;

        vec3 l = vec3(0.0, 1.0, 0.0);
        float attenuation = tbx_compute_distance_attenuation(light.position, world_pos, light.range, l);
        float area_factor = tbx_compute_area_factor(light.area_size);

        vec3 brdf = tbx_evaluate_pbr_brdf(n, v, l, albedo, metallic, roughness, f0, n_dot_v);
        if (dot(brdf, brdf) <= 0.0)
            continue;

        vec3 radiance = light_color * intensity * attenuation * area_factor;
        direct += brdf * radiance;
    }

    int directional_count = clamp(u_directional_light_count, 0, TBX_MAX_DIRECTIONAL_LIGHTS);
    for (int i = 0; i < directional_count; ++i)
    {
        TbxDirectionalLight light = u_directional_lights[i];
        float intensity = max(light.intensity, 0.0);
        if (intensity <= 0.0)
            continue;

        vec3 light_color = tbx_normalize_light_color(light.color);
        if (dot(light_color, light_color) <= 0.0)
            continue;

        vec3 l = normalize(-light.direction);

        vec3 brdf = tbx_evaluate_pbr_brdf(n, v, l, albedo, metallic, roughness, f0, n_dot_v);
        if (dot(brdf, brdf) <= 0.0)
            continue;

        vec3 radiance = light_color * intensity;
        direct += brdf * radiance;
    }

    vec3 ambient = u_ambient_light * albedo * (1.0 - metallic);
    return ambient + direct;
}

vec3 tbx_compute_lighting(vec3 world_pos, vec3 normal)
{
    vec3 lighting = u_ambient_light;

    int point_count = clamp(u_point_light_count, 0, TBX_MAX_POINT_LIGHTS);
    for (int i = 0; i < point_count; ++i)
    {
        TbxPointLight light = u_point_lights[i];
        vec3 light_color = tbx_normalize_light_color(light.color);
        vec3 light_dir = vec3(0.0, 1.0, 0.0);
        float attenuation = tbx_compute_distance_attenuation(light.position, world_pos, light.range, light_dir);

        float n_dot_l = max(dot(normal, light_dir), 0.0);
        if (n_dot_l <= 0.0)
            continue;

        lighting += light_color * light.intensity * attenuation * n_dot_l;
    }

    int spot_count = clamp(u_spot_light_count, 0, TBX_MAX_SPOT_LIGHTS);
    for (int i = 0; i < spot_count; ++i)
    {
        TbxSpotLight light = u_spot_lights[i];
        vec3 light_color = tbx_normalize_light_color(light.color);
        vec3 light_dir = vec3(0.0, 1.0, 0.0);
        float attenuation = tbx_compute_distance_attenuation(light.position, world_pos, light.range, light_dir);
        float spot_factor =
            tbx_compute_spot_factor(light.position, light.direction, world_pos, light.inner_cos, light.outer_cos);

        float n_dot_l = max(dot(normal, light_dir), 0.0);
        if (n_dot_l <= 0.0)
            continue;

        lighting += light_color * light.intensity * attenuation * n_dot_l * spot_factor;
    }

    int area_count = clamp(u_area_light_count, 0, TBX_MAX_AREA_LIGHTS);
    for (int i = 0; i < area_count; ++i)
    {
        TbxAreaLight light = u_area_lights[i];
        vec3 light_color = tbx_normalize_light_color(light.color);
        vec3 light_dir = vec3(0.0, 1.0, 0.0);
        float attenuation = tbx_compute_distance_attenuation(light.position, world_pos, light.range, light_dir);
        float area_factor = tbx_compute_area_factor(light.area_size);

        float n_dot_l = max(dot(normal, light_dir), 0.0);
        if (n_dot_l <= 0.0)
            continue;

        lighting += light_color * light.intensity * attenuation * n_dot_l * area_factor;
    }

    int directional_count = clamp(u_directional_light_count, 0, TBX_MAX_DIRECTIONAL_LIGHTS);
    for (int i = 0; i < directional_count; ++i)
    {
        TbxDirectionalLight light = u_directional_lights[i];
        vec3 light_color = tbx_normalize_light_color(light.color);
        vec3 light_dir = normalize(-light.direction);

        float n_dot_l = max(dot(normal, light_dir), 0.0);
        if (n_dot_l <= 0.0)
            continue;

        lighting += light_color * light.intensity * n_dot_l;
    }

    return lighting;
}

