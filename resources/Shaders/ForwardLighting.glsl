const int TBX_MAX_FORWARD_DIRECTIONAL_LIGHTS = 4;
const int TBX_MAX_FORWARD_POINT_LIGHTS = 16;
const int TBX_MAX_FORWARD_SPOT_LIGHTS = 8;
const int TBX_MAX_FORWARD_AREA_LIGHTS = 4;

struct TbxForwardDirectionalLight
{
    vec4 direction_ambient;
    vec4 radiance;
    ivec4 shadow_info;
};

struct TbxForwardPointLight
{
    vec4 position_range;
    vec4 radiance_shadow_bias;
    ivec4 shadow_info;
};

struct TbxForwardSpotLight
{
    vec4 position_range;
    vec4 direction_inner_cos;
    vec4 radiance_outer_cos;
    ivec4 shadow_info;
};

struct TbxForwardAreaLight
{
    vec4 position_range;
    vec4 direction_half_width;
    vec4 radiance_half_height;
    vec4 right;
    vec4 up;
    ivec4 shadow_info;
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

const int TBX_MAX_FORWARD_SHADOW_CASCADES = 3;
const int TBX_MAX_FORWARD_POINT_SHADOWS = TBX_MAX_FORWARD_POINT_LIGHTS;
const int TBX_MAX_FORWARD_SPOT_SHADOWS = TBX_MAX_FORWARD_SPOT_LIGHTS;
const int TBX_MAX_FORWARD_AREA_SHADOWS = TBX_MAX_FORWARD_AREA_LIGHTS;

struct TbxForwardShadowCascade
{
    mat4 light_view_projection;
    vec4 split_bias_blend;
};

struct TbxForwardPointShadowMap
{
    mat4 world_to_light;
    vec4 range_and_bias;
    ivec4 layers;
};

struct TbxForwardProjectedShadowMap
{
    mat4 light_view_projection;
    vec4 planes_and_bias;
    ivec4 texture_layer;
};

layout(std140, binding = 8) uniform ToyboxShadowBlock
{
    ivec4 u_tbx_shadow_counts;
    vec4 u_tbx_shadow_settings;
    mat4 u_tbx_shadow_camera_view;
    TbxForwardShadowCascade u_tbx_directional_shadow_cascades[TBX_MAX_FORWARD_SHADOW_CASCADES];
    TbxForwardPointShadowMap u_tbx_point_shadow_maps_data[TBX_MAX_FORWARD_POINT_SHADOWS];
    TbxForwardProjectedShadowMap u_tbx_spot_shadow_maps_data[TBX_MAX_FORWARD_SPOT_SHADOWS];
    TbxForwardProjectedShadowMap u_tbx_area_shadow_maps_data[TBX_MAX_FORWARD_AREA_SHADOWS];
};

layout(binding = 8) uniform sampler2D
    u_tbx_directional_shadow_maps[TBX_MAX_FORWARD_SHADOW_CASCADES];
layout(binding = 11) uniform sampler2DArray u_tbx_point_shadow_maps;
layout(binding = 12) uniform sampler2DArray u_tbx_spot_shadow_maps;
layout(binding = 13) uniform sampler2DArray u_tbx_area_shadow_maps;

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

    vec3 half_input = view_direction + light_direction;
    vec3 half_vector = normalize(half_input + (vec3(0.0, 0.0, 1.0) * step(length(half_input), 0.0001)));
    float specular_power = pow(max(dot(normal, half_vector), 0.0), shininess);
    vec3 light_energy = radiance * (n_dot_l * attenuation);
    diffuse_accumulation += albedo * light_energy;
    specular_accumulation += vec3(specular_strength * specular_power) * light_energy;
}

float tbx_forward_shadow_normal_offset(vec3 normal, vec3 light_direction, float normal_bias)
{
    float ndotl = max(dot(normal, light_direction), 0.0);
    float slope_factor = 1.0 - ndotl;
    return slope_factor * slope_factor * normal_bias;
}

bool tbx_try_project_forward_shadow(vec4 shadow_position, out vec3 projected)
{
    if (shadow_position.w <= 0.0)
        return false;

    projected = shadow_position.xyz / shadow_position.w;
    projected = (projected * 0.5) + 0.5;
    return projected.x > 0.0 && projected.x < 1.0 && projected.y > 0.0 && projected.y < 1.0
           && projected.z > 0.0 && projected.z < 1.0;
}

float tbx_sample_forward_shadow_map(
    int cascade_index,
    vec4 shadow_position,
    float depth_bias)
{
    vec3 projected = vec3(0.0);
    if (!tbx_try_project_forward_shadow(shadow_position, projected))
        return 1.0;

    vec2 texel_size = 1.0 / vec2(textureSize(u_tbx_directional_shadow_maps[cascade_index], 0));
    float filter_radius = max(u_tbx_shadow_settings.x, 1.0);
    vec2 sample_margin = min(texel_size * filter_radius, vec2(0.49));
    vec2 sample_center = clamp(projected.xy, sample_margin, vec2(1.0) - sample_margin);
    float current_depth = projected.z - depth_bias;
    float visibility = 0.0;
    for (int sample_y = -1; sample_y <= 1; ++sample_y)
        for (int sample_x = -1; sample_x <= 1; ++sample_x)
        {
            vec2 sample_uv =
                sample_center + (vec2(sample_x, sample_y) * texel_size * filter_radius);
            float stored_depth = texture(u_tbx_directional_shadow_maps[cascade_index], sample_uv).r;
            visibility += step(current_depth, stored_depth);
        }

    return visibility / 9.0;
}

float tbx_sample_forward_projected_shadow_map(
    sampler2DArray shadow_texture,
    vec4 shadow_position,
    int texture_layer,
    float depth_bias)
{
    if (texture_layer < 0 || shadow_position.w <= 0.0)
        return 1.0;

    vec3 projected = shadow_position.xyz / shadow_position.w;
    projected = (projected * 0.5) + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0 || projected.y <= 0.0 || projected.y >= 1.0
        || projected.z <= 0.0 || projected.z >= 1.0)
        return 1.0;

    vec2 texel_size = 1.0 / vec2(textureSize(shadow_texture, 0).xy);
    float filter_radius = max(u_tbx_shadow_settings.x, 1.0);
    vec2 sample_margin = min(texel_size * filter_radius, vec2(0.49));
    vec2 sample_center = clamp(projected.xy, sample_margin, vec2(1.0) - sample_margin);
    float current_depth = projected.z - depth_bias;
    float visibility = 0.0;
    for (int sample_y = -1; sample_y <= 1; ++sample_y)
        for (int sample_x = -1; sample_x <= 1; ++sample_x)
        {
            vec2 sample_uv =
                sample_center + (vec2(sample_x, sample_y) * texel_size * filter_radius);
            float stored_depth = texture(shadow_texture, vec3(sample_uv, float(texture_layer))).r;
            visibility += step(current_depth, stored_depth);
        }
    return visibility / 9.0;
}

float tbx_sample_forward_directional_shadow(
    TbxForwardDirectionalLight light,
    vec3 world_position,
    vec3 normal,
    vec3 light_direction)
{
    int cascade_count = min(light.shadow_info.y, u_tbx_shadow_counts.x);
    if (cascade_count <= 0)
        return 1.0;

    float view_depth = -(u_tbx_shadow_camera_view * vec4(world_position, 1.0)).z;
    for (int cascade_index = 0; cascade_index < cascade_count; ++cascade_index)
    {
        TbxForwardShadowCascade cascade = u_tbx_directional_shadow_cascades[cascade_index];
        float normal_offset = tbx_forward_shadow_normal_offset(
            normal,
            light_direction,
            cascade.split_bias_blend.y);
        vec4 shadow_position =
            cascade.light_view_projection * vec4(world_position + (normal * normal_offset), 1.0);
        vec3 projected = vec3(0.0);
        bool is_inside_cascade = tbx_try_project_forward_shadow(shadow_position, projected);
        float visibility = tbx_sample_forward_shadow_map(
            cascade_index,
            shadow_position,
            cascade.split_bias_blend.z);

        float split_depth = cascade.split_bias_blend.x;
        float blend_distance = cascade.split_bias_blend.w;
        bool is_last_cascade = cascade_index == cascade_count - 1;
        if (!is_last_cascade && view_depth >= split_depth - blend_distance
            && view_depth <= split_depth)
        {
            TbxForwardShadowCascade next_cascade =
                u_tbx_directional_shadow_cascades[cascade_index + 1];
            float next_normal_offset = tbx_forward_shadow_normal_offset(
                normal,
                light_direction,
                next_cascade.split_bias_blend.y);
            vec4 next_shadow_position = next_cascade.light_view_projection
                                        * vec4(world_position + (normal * next_normal_offset), 1.0);
            vec3 next_projected = vec3(0.0);
            bool is_inside_next_cascade =
                tbx_try_project_forward_shadow(next_shadow_position, next_projected);
            float next_visibility = tbx_sample_forward_shadow_map(
                cascade_index + 1,
                next_shadow_position,
                next_cascade.split_bias_blend.z);
            if (!is_inside_cascade)
                return is_inside_next_cascade ? next_visibility : 1.0;
            if (!is_inside_next_cascade)
                return visibility;

            float blend = clamp(
                (view_depth - (split_depth - blend_distance)) / max(blend_distance, 0.0001),
                0.0,
                1.0);
            return mix(visibility, next_visibility, blend);
        }

        if (view_depth <= split_depth || is_last_cascade)
            return is_inside_cascade ? visibility : 1.0;
    }

    return 1.0;
}

float tbx_sample_forward_projected_shadow(
    sampler2DArray shadow_texture,
    TbxForwardProjectedShadowMap shadow_map,
    vec3 world_position,
    vec3 normal,
    vec3 light_direction)
{
    float normal_offset = tbx_forward_shadow_normal_offset(
        normal,
        light_direction,
        shadow_map.planes_and_bias.z);
    vec4 shadow_position = shadow_map.light_view_projection
                           * vec4(world_position + (normal * normal_offset), 1.0);
    return tbx_sample_forward_projected_shadow_map(
        shadow_texture,
        shadow_position,
        shadow_map.texture_layer.x,
        shadow_map.planes_and_bias.w);
}

float tbx_sample_forward_point_shadow(
    TbxForwardPointLight light,
    vec3 world_position,
    vec3 normal)
{
    int shadow_map_index = light.shadow_info.x;
    if (shadow_map_index < 0 || shadow_map_index >= u_tbx_shadow_counts.y)
        return 1.0;

    TbxForwardPointShadowMap shadow_map = u_tbx_point_shadow_maps_data[shadow_map_index];
    vec3 local_position = (shadow_map.world_to_light * vec4(world_position, 1.0)).xyz;
    float distance_to_light = length(local_position);
    float light_range = max(shadow_map.range_and_bias.x, 0.0001);
    if (distance_to_light <= 0.0001 || distance_to_light >= light_range)
        return 1.0;

    vec3 local_direction = local_position / distance_to_light;
    int hemisphere = local_direction.z >= 0.0 ? 0 : 1;
    float denominator =
        hemisphere == 0 ? max(1.0 + local_direction.z, 0.0001)
                        : max(1.0 - local_direction.z, 0.0001);
    vec2 projected_uv = (local_direction.xy / denominator) * 0.5 + 0.5;
    if (projected_uv.x <= 0.0 || projected_uv.x >= 1.0 || projected_uv.y <= 0.0
        || projected_uv.y >= 1.0)
        return 1.0;

    vec3 world_to_light = light.position_range.xyz - world_position;
    vec3 world_direction = normalize(world_to_light);
    float facing = max(dot(normal, world_direction), 0.0);
    float bias = (1.0 - facing)
                 * max(light.radiance_shadow_bias.w, shadow_map.range_and_bias.z);
    float current_depth = clamp((distance_to_light - bias) / light_range, 0.0, 1.0);
    vec2 texel_size = 1.0 / vec2(textureSize(u_tbx_point_shadow_maps, 0).xy);
    float filter_radius = max(u_tbx_shadow_settings.x, 1.0);
    vec2 sample_margin = min(texel_size * filter_radius, vec2(0.49));
    vec2 sample_center = clamp(projected_uv, sample_margin, vec2(1.0) - sample_margin);
    float visibility = 0.0;
    float layer = float(shadow_map.layers.x + hemisphere);
    for (int sample_y = -2; sample_y <= 2; ++sample_y)
        for (int sample_x = -2; sample_x <= 2; ++sample_x)
        {
            vec2 sample_uv =
                sample_center + (vec2(sample_x, sample_y) * texel_size * filter_radius);
            float stored_depth =
                texture(u_tbx_point_shadow_maps, vec3(sample_uv, layer)).r;
            visibility += step(current_depth, stored_depth);
        }

    return visibility / 25.0;
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
        float shadow_visibility = tbx_sample_forward_directional_shadow(
            light,
            world_position,
            normalized_normal,
            light_direction);
        ambient_accumulation += albedo * light.radiance.rgb * light.direction_ambient.w;
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance.rgb,
            shadow_visibility,
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
        float shadow_visibility =
            tbx_sample_forward_point_shadow(light, world_position, normalized_normal);
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance_shadow_bias.rgb,
            attenuation * shadow_visibility,
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
        float shadow_visibility = 1.0;
        if (light.shadow_info.x >= 0 && light.shadow_info.x < u_tbx_shadow_counts.z)
        {
            shadow_visibility = tbx_sample_forward_projected_shadow(
                u_tbx_spot_shadow_maps,
                u_tbx_spot_shadow_maps_data[light.shadow_info.x],
                world_position,
                normalized_normal,
                light_direction);
        }
        float attenuation =
            tbx_forward_distance_attenuation(distance_squared, light.position_range.w);
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance_outer_cos.rgb,
            attenuation * cone * cone * cone_mask * shadow_visibility,
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
        float shadow_visibility = 1.0;
        if (light.shadow_info.x >= 0 && light.shadow_info.x < u_tbx_shadow_counts.w)
        {
            shadow_visibility = tbx_sample_forward_projected_shadow(
                u_tbx_area_shadow_maps,
                u_tbx_area_shadow_maps_data[light.shadow_info.x],
                world_position,
                normalized_normal,
                light_direction);
        }
        tbx_accumulate_forward_light(
            albedo,
            normalized_normal,
            view_direction,
            light_direction,
            light.radiance_half_height.rgb,
            attenuation * shadow_visibility,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    vec3 lit_color = ambient_accumulation + diffuse_accumulation + specular_accumulation + emissive;
    return clamp(lit_color, 0.0, 1.0);
}
