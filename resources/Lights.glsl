#pragma once

const int TBX_LIGHT_POINT = 0;
const int TBX_LIGHT_AREA = 1;
const int TBX_LIGHT_SPOT = 2;
const int TBX_LIGHT_DIRECTIONAL = 3;
const int TBX_MAX_LIGHTS = 16;

struct TbxLight
{
    int mode;
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float inner_cos;
    float outer_cos;
    vec2 area_size;
};

uniform int u_light_count;
uniform TbxLight u_lights[TBX_MAX_LIGHTS];
uniform vec3 u_ambient_light = vec3(0.03, 0.03, 0.03);

vec3 tbx_compute_lighting(vec3 world_pos, vec3 normal)
{
    vec3 lighting = u_ambient_light;
    int count = clamp(u_light_count, 0, TBX_MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
    {
        TbxLight light = u_lights[i];
        vec3 light_dir = vec3(0.0, 1.0, 0.0);
        float attenuation = 1.0;
        if (light.mode == TBX_LIGHT_DIRECTIONAL)
        {
            light_dir = normalize(-light.direction);
        }
        else
        {
            vec3 to_light = light.position - world_pos;
            float distance = length(to_light);
            if (distance > 0.0001)
            {
                light_dir = to_light / distance;
            }
            float inv_range = 1.0 / max(light.range, 0.001);
            float falloff = clamp(1.0 - distance * inv_range, 0.0, 1.0);
            attenuation = falloff * falloff;
        }

        float n_dot_l = max(dot(normal, light_dir), 0.0);
        if (n_dot_l <= 0.0)
        {
            continue;
        }

        float spot_factor = 1.0;
        if (light.mode == TBX_LIGHT_SPOT)
        {
            vec3 to_surface = normalize(world_pos - light.position);
            float spot_cos = dot(normalize(light.direction), to_surface);
            spot_factor = smoothstep(light.outer_cos, light.inner_cos, spot_cos);
        }

        float area_factor = 1.0;
        if (light.mode == TBX_LIGHT_AREA)
        {
            float average_extent = 0.5 * (light.area_size.x + light.area_size.y);
            area_factor = max(1.0, average_extent);
        }

        vec3 contribution =
            light.color * light.intensity * attenuation * n_dot_l * spot_factor * area_factor;
        lighting += contribution;
    }

    return lighting;
}
