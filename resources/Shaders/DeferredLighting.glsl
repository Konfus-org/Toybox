struct DirectionalLight
{
    vec3 direction;
    float ambient_intensity;
    vec3 radiance;
    float casts_shadows;
};

struct PointLight
{
    vec3 position;
    float range;
    vec3 radiance;
    float padding;
};

struct SpotLight
{
    vec3 position;
    float range;
    vec3 direction;
    float inner_cos;
    vec3 radiance;
    float outer_cos;
};

struct AreaLight
{
    vec3 position;
    float range;
    vec3 direction;
    float half_width;
    vec3 radiance;
    float half_height;
    vec3 right;
    float padding0;
    vec3 up;
    float padding1;
};

struct TileLightSpan
{
    uint point_offset;
    uint point_count;
    uint spot_offset;
    uint spot_count;
    uint area_offset;
    uint area_count;
    uint padding0;
    uint padding1;
};

struct LightingInfo
{
    vec4 camera_position;
    vec4 clear_color;
    ivec4 tile_info;
    ivec4 light_counts;
    mat4 inverse_view_projection;
    DirectionalLight directional_lights[4];
};

vec3 tbx_reconstruct_world_position(vec2 uv, float depth, mat4 inverse_view_projection)
{
    vec4 clip_position = vec4((uv * 2.0) - 1.0, (depth * 2.0) - 1.0, 1.0);
    vec4 world_position = inverse_view_projection * clip_position;
    return world_position.xyz / max(world_position.w, 0.0001);
}

float tbx_get_distance_attenuation(float distance_squared, float range)
{
    float range_squared = max(range * range, 0.0001);
    float active = 1.0 - step(range_squared, distance_squared);
    float normalized_distance_squared = distance_squared / range_squared;
    float range_falloff = clamp(1.0 - normalized_distance_squared, 0.0, 1.0);
    return active * (range_falloff * range_falloff) / max(distance_squared, 0.25);
}

vec3 tbx_get_area_light_closest_point(AreaLight light, vec3 world_position)
{
    vec3 relative_to_center = world_position - light.position;
    float local_x =
        clamp(dot(relative_to_center, light.right), -light.half_width, light.half_width);
    float local_y = clamp(dot(relative_to_center, light.up), -light.half_height, light.half_height);
    return light.position + (light.right * local_x) + (light.up * local_y);
}

float tbx_get_area_light_attenuation(AreaLight light, vec3 light_direction, float distance_squared)
{
    float base_attenuation = tbx_get_distance_attenuation(distance_squared, light.range);
    float front_face = max(dot(light.direction, -light_direction), 0.0);
    float emitter_extent = max(light.half_width + light.half_height, 0.001);
    float softness = emitter_extent / (emitter_extent + sqrt(max(distance_squared, 0.0001)));
    return base_attenuation * front_face * (0.35 + (0.65 * softness));
}

void tbx_accumulate_light(
    vec3 albedo,
    vec3 normal,
    vec3 view_direction,
    vec3 light_direction,
    vec3 radiance,
    float attenuation,
    float specular_strength,
    float shininess,
    inout vec3 diffuse_accumulation,
    inout vec3 specular_accumulation)
{
    float n_dot_l = max(dot(normal, light_direction), 0.0);
    float active = step(0.0001, attenuation) * step(0.0001, n_dot_l);
    vec3 half_vector = normalize(view_direction + light_direction);
    float specular_power = pow(max(dot(normal, half_vector), 0.0), shininess);
    vec3 light_energy = radiance * (n_dot_l * attenuation * active);
    diffuse_accumulation += albedo * light_energy;
    specular_accumulation += vec3(specular_strength * specular_power) * light_energy;
}
