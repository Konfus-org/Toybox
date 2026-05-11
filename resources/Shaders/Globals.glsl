layout(std140, binding = 0) uniform ToyboxViewBlock
{
    mat4 u_view_proj;
};

layout(std140, binding = 1) uniform ToyboxMaterialBlock
{
    vec4 u_material_uniforms[64];
};

mat4 tbx_get_model_matrix(const mat4 model)
{
    return model;
}

uint tbx_get_instance_id(const uint instance_id)
{
    return instance_id;
}

vec3 tbx_srgb_to_linear(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(2.2));
}

vec3 tbx_linear_to_srgb(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 tbx_tonemap_aces(vec3 color)
{
    // ACES fitted tonemap (Narkowicz 2015)
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float tbx_interleaved_gradient_noise(vec2 pixel_coord)
{
    return fract(52.9829189 * fract(dot(pixel_coord, vec2(0.06711056, 0.00583715))));
}

const int TBX_MAX_FORWARD_DIRECTIONAL_LIGHTS = 4;
const int TBX_MAX_FORWARD_POINT_LIGHTS = 16;
const int TBX_MAX_FORWARD_SPOT_LIGHTS = 8;
const int TBX_MAX_FORWARD_AREA_LIGHTS = 4;

struct TbxForwardDirectionalLight
{
    vec4 direction_ambient;
    vec4 radiance;
};

struct TbxForwardPointLight
{
    vec4 position_range;
    vec4 radiance;
};

struct TbxForwardSpotLight
{
    vec4 position_range;
    vec4 direction_inner_cos;
    vec4 radiance_outer_cos;
};

struct TbxForwardAreaLight
{
    vec4 position_range;
    vec4 direction_half_width;
    vec4 radiance_half_height;
    vec4 right;
    vec4 up;
};

layout(std140, binding = 7) uniform ToyboxLightingBlock
{
    vec4 u_tbx_camera_position;
    ivec4 u_tbx_light_counts;
    TbxForwardDirectionalLight u_tbx_directional_lights[TBX_MAX_FORWARD_DIRECTIONAL_LIGHTS];
    TbxForwardPointLight u_tbx_point_lights[TBX_MAX_FORWARD_POINT_LIGHTS];
    TbxForwardSpotLight u_tbx_spot_lights[TBX_MAX_FORWARD_SPOT_LIGHTS];
    TbxForwardAreaLight u_tbx_area_lights[TBX_MAX_FORWARD_AREA_LIGHTS];
};

float tbx_forward_distance_attenuation(float distance_squared, float range)
{
    float range_squared = max(range * range, 0.0001);
    float attenuation_mask = 1.0 - step(range_squared, distance_squared);
    float normalized_distance_squared = distance_squared / range_squared;
    float range_falloff = clamp(1.0 - normalized_distance_squared, 0.0, 1.0);
    return attenuation_mask * (range_falloff * range_falloff) / max(distance_squared, 0.25);
}

vec3 tbx_forward_area_light_closest_point(
    TbxForwardAreaLight light,
    vec3 world_position)
{
    vec3 relative_to_center = world_position - light.position_range.xyz;
    float local_x = clamp(
        dot(relative_to_center, light.right.xyz),
        -light.direction_half_width.w,
        light.direction_half_width.w);
    float local_y = clamp(
        dot(relative_to_center, light.up.xyz),
        -light.radiance_half_height.w,
        light.radiance_half_height.w);
    return light.position_range.xyz + (light.right.xyz * local_x) + (light.up.xyz * local_y);
}

float tbx_forward_area_attenuation(
    TbxForwardAreaLight light,
    vec3 light_direction,
    float distance_squared)
{
    float base_attenuation =
        tbx_forward_distance_attenuation(distance_squared, light.position_range.w);
    float front_face = max(dot(light.direction_half_width.xyz, -light_direction), 0.0);
    float emitter_extent = max(light.direction_half_width.w + light.radiance_half_height.w, 0.001);
    float softness = emitter_extent / (emitter_extent + sqrt(max(distance_squared, 0.0001)));
    return base_attenuation * front_face * (0.35 + (0.65 * softness));
}

void tbx_accumulate_forward_light(
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
    if (attenuation <= 0.0001 || n_dot_l <= 0.0001)
        return;

    vec3 half_vector = normalize(view_direction + light_direction);
    float specular_power = pow(max(dot(normal, half_vector), 0.0), shininess);
    vec3 light_energy = radiance * (n_dot_l * attenuation);
    diffuse_accumulation += albedo * light_energy;
    specular_accumulation += vec3(specular_strength * specular_power) * light_energy;
}

vec3 tbx_apply_forward_lighting(
    vec3 albedo,
    vec3 emissive,
    vec3 world_position,
    vec3 normal,
    float specular_strength,
    float shininess)
{
    int light_count =
        u_tbx_light_counts.x + u_tbx_light_counts.y + u_tbx_light_counts.z + u_tbx_light_counts.w;
    if (light_count <= 0)
        return clamp(albedo + emissive, 0.0, 1.0);

    vec3 normalized_normal = normalize(normal);
    vec3 view_delta = u_tbx_camera_position.xyz - world_position;
    vec3 view_direction = normalize(view_delta + vec3(0.0, 0.0, step(length(view_delta), 0.0001)));
    vec3 diffuse_accumulation = vec3(0.0);
    vec3 specular_accumulation = vec3(0.0);
    float hemisphere_factor = clamp((normalized_normal.y * 0.5) + 0.5, 0.0, 1.0);
    vec3 ambient_accumulation =
        albedo * mix(vec3(0.05, 0.045, 0.04), vec3(0.17, 0.19, 0.23), hemisphere_factor);

    for (int light_index = 0; light_index < u_tbx_light_counts.x; ++light_index)
    {
        TbxForwardDirectionalLight light = u_tbx_directional_lights[light_index];
        vec3 light_direction = normalize(-light.direction_ambient.xyz);
        ambient_accumulation += albedo * light.radiance.rgb * light.direction_ambient.w;
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance.rgb,
            1.0,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (int light_index = 0; light_index < u_tbx_light_counts.y; ++light_index)
    {
        TbxForwardPointLight light = u_tbx_point_lights[light_index];
        vec3 to_light = light.position_range.xyz - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float attenuation =
            tbx_forward_distance_attenuation(distance_squared, light.position_range.w);
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance.rgb,
            attenuation,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (int light_index = 0; light_index < u_tbx_light_counts.z; ++light_index)
    {
        TbxForwardSpotLight light = u_tbx_spot_lights[light_index];
        vec3 to_light = light.position_range.xyz - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float spot_cos = dot(light.direction_inner_cos.xyz, -light_direction);
        float cone = clamp(
            (spot_cos - light.radiance_outer_cos.w)
                / max(light.direction_inner_cos.w - light.radiance_outer_cos.w, 0.0001),
            0.0,
            1.0);
        float cone_mask = step(light.radiance_outer_cos.w, spot_cos);
        float attenuation =
            tbx_forward_distance_attenuation(distance_squared, light.position_range.w);
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance_outer_cos.rgb,
            attenuation * cone * cone * cone_mask,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (int light_index = 0; light_index < u_tbx_light_counts.w; ++light_index)
    {
        TbxForwardAreaLight light = u_tbx_area_lights[light_index];
        vec3 closest_point = tbx_forward_area_light_closest_point(light, world_position);
        vec3 to_light = closest_point - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = mix(
            -light.direction_half_width.xyz,
            to_light * inversesqrt(max(distance_squared, 0.0001)),
            step(0.0001, distance_squared));
        float attenuation = tbx_forward_area_attenuation(light, light_direction, distance_squared);
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance_half_height.rgb,
            attenuation,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    vec3 lit_color = ambient_accumulation + diffuse_accumulation + specular_accumulation + emissive;
    return clamp(lit_color, 0.0, 1.0);
}
