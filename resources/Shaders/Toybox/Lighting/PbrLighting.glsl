#include "Toybox/ShaderBase.glsl"
#include "Toybox/Lighting/PbrSurface.glsl"
#include "Toybox/Lighting/ShadowSampling.glsl"

float tbx_distribution_ggx(vec3 normal, vec3 half_vector, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float ndh = max(dot(normal, half_vector), 0.0);
    float ndh2 = ndh * ndh;

    float denominator = ndh2 * (a2 - 1.0) + 1.0;
    denominator = TBX_PI * denominator * denominator;

    return a2 / max(denominator, TBX_EPSILON);
}

float tbx_geometry_schlick_ggx(float ndv, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return ndv / max(ndv * (1.0 - k) + k, TBX_EPSILON);
}

float tbx_geometry_smith(vec3 normal, vec3 view_dir, vec3 light_dir, float roughness)
{
    float ndv = max(dot(normal, view_dir), 0.0);
    float ndl = max(dot(normal, light_dir), 0.0);

    return tbx_geometry_schlick_ggx(ndv, roughness) *
           tbx_geometry_schlick_ggx(ndl, roughness);
}

vec3 tbx_fresnel_schlick(float cos_theta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float tbx_point_attenuation(float distance_to_light, float radius)
{
    float attenuation = clamp(1.0 - distance_to_light / max(radius, TBX_EPSILON), 0.0, 1.0);
    return attenuation * attenuation;
}

float tbx_spot_attenuation(vec3 light_dir, vec3 spot_dir, float inner_cos, float outer_cos)
{
    float theta = dot(light_dir, normalize(-spot_dir));
    float epsilon = max(inner_cos - outer_cos, TBX_EPSILON);
    return clamp((theta - outer_cos) / epsilon, 0.0, 1.0);
}

vec3 tbx_evaluate_pbr_brdf(
    vec3 normal,
    vec3 view_dir,
    vec3 light_dir,
    vec3 radiance,
    vec3 albedo,
    float metallic,
    float roughness)
{
    vec3 half_vector = normalize(view_dir + light_dir);

    float ndl = max(dot(normal, light_dir), 0.0);
    float ndv = max(dot(normal, view_dir), 0.0);

    if (ndl <= 0.0 || ndv <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    float normal_distribution = tbx_distribution_ggx(normal, half_vector, roughness);
    float geometry = tbx_geometry_smith(normal, view_dir, light_dir, roughness);
    vec3 fresnel = tbx_fresnel_schlick(max(dot(half_vector, view_dir), 0.0), f0);

    vec3 numerator = normal_distribution * geometry * fresnel;
    float denominator = max(4.0 * ndv * ndl, TBX_EPSILON);

    vec3 specular = numerator / denominator;

    vec3 ks = fresnel;
    vec3 kd = vec3(1.0) - ks;
    kd *= 1.0 - metallic;

    vec3 diffuse = kd * albedo / TBX_PI;

    return (diffuse + specular) * radiance * ndl;
}

vec3 tbx_evaluate_light_pbr(TbxLight light, PbrSurface surface, vec3 view_dir)
{
    int light_type = int(light.position_type.w);

    vec3 light_dir;
    float attenuation = 1.0;

    if (light_type == TBX_LIGHT_TYPE_DIRECTIONAL)
    {
        light_dir = normalize(-light.direction_range.xyz);
    }
    else
    {
        vec3 light_vector = light.position_type.xyz - surface.world_position;
        float distance_to_light = length(light_vector);

        light_dir = light_vector / max(distance_to_light, TBX_EPSILON);
        attenuation = tbx_point_attenuation(distance_to_light, light.direction_range.w);

        if (light_type == TBX_LIGHT_TYPE_SPOT)
        {
            attenuation *= tbx_spot_attenuation(
                light_dir,
                light.direction_range.xyz,
                light.params.x,
                light.params.y);
        }
    }

    vec3 radiance = light.color_intensity.rgb * light.color_intensity.w * attenuation;

    return tbx_evaluate_pbr_brdf(
        surface.normal,
        view_dir,
        light_dir,
        radiance,
        surface.albedo,
        surface.metallic,
        surface.roughness);
}

vec3 tbx_get_shadow_light_direction(TbxLight light, PbrSurface surface)
{
    if (int(light.position_type.w) == TBX_LIGHT_TYPE_DIRECTIONAL)
    {
        return light.direction_range.xyz;
    }

    return normalize(surface.world_position - light.position_type.xyz);
}

vec3 tbx_shade_pbr(PbrSurface surface)
{
    vec3 view_dir = normalize(u_camera_world_position.xyz - surface.world_position);
    vec3 color = u_ambient_color.rgb * surface.albedo;

    for (int i = 0; i < u_light_meta.x; ++i)
    {
        vec3 contribution = tbx_evaluate_light_pbr(u_lights[i], surface, view_dir);
        if (u_lights[i].params.z >= 0.0)
        {
            contribution *= tbx_sample_shadow(
                surface.world_position,
                surface.normal,
                tbx_get_shadow_light_direction(u_lights[i], surface),
                int(u_lights[i].params.z),
                int(max(u_lights[i].params.w, 1.0)));
        }

        color += contribution;
    }

    color += surface.emissive;
    return color;
}
