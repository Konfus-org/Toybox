#version 450 core

in vec2 v_tex_coord;

layout(location = 0) out vec4 o_final_color;

uniform sampler2D u_albedo;
uniform sampler2D u_normal;
uniform sampler2D u_emissive;
uniform sampler2D u_material;
uniform sampler2D u_depth;
uniform sampler2DArray u_directional_shadows;
uniform samplerCubeArray u_point_shadows;
uniform sampler2DArray u_spot_shadows;
uniform sampler2DArray u_area_shadows;

struct DirectionalLight
{
    vec4 direction_ambient;
    vec4 radiance_shadowed;
    ivec4 shadow_info;
};

struct PointLight
{
    vec4 position_range;
    vec4 radiance_shadow_bias;
    ivec4 shadow_info;
};

struct SpotLight
{
    vec4 position_range;
    vec4 direction_inner_cos;
    vec4 radiance_outer_cos;
    ivec4 shadow_info;
};

struct AreaLight
{
    vec4 position_range;
    vec4 direction_half_width;
    vec4 radiance_half_height;
    vec4 right;
    vec4 up;
    ivec4 shadow_info;
};

struct TileLightSpan
{
    uvec4 point_and_spot;
    uvec4 area;
};

struct ShadowCascade
{
    mat4 light_view_projection;
    vec4 split_and_bias;
    ivec4 texture_layer;
};

struct ProjectedShadow
{
    mat4 light_view_projection;
    vec4 planes_and_bias;
    ivec4 texture_layer;
};

struct LightingInfo
{
    vec4 camera_position;
    vec4 clear_color;
    ivec4 tile_info;
    ivec4 light_counts;
    mat4 inverse_view_projection;
    mat4 view_matrix;
    DirectionalLight directional_lights[4];
};

layout(std140, binding = 7) uniform LightingInfoBuffer
{
    LightingInfo u_lighting_info;
};

layout(std430, binding = 0) readonly buffer PointLightsBuffer
{
    PointLight u_point_lights[];
};

layout(std430, binding = 1) readonly buffer SpotLightsBuffer
{
    SpotLight u_spot_lights[];
};

layout(std430, binding = 2) readonly buffer TileLightSpansBuffer
{
    TileLightSpan u_tile_light_spans[];
};

layout(std430, binding = 3) readonly buffer TilePointLightIndicesBuffer
{
    uint u_tile_point_light_indices[];
};

layout(std430, binding = 4) readonly buffer TileSpotLightIndicesBuffer
{
    uint u_tile_spot_light_indices[];
};

layout(std430, binding = 5) readonly buffer AreaLightsBuffer
{
    AreaLight u_area_lights[];
};

layout(std430, binding = 6) readonly buffer TileAreaLightIndicesBuffer
{
    uint u_tile_area_light_indices[];
};

layout(std430, binding = 8) readonly buffer DirectionalShadowCascadesBuffer
{
    ShadowCascade u_directional_shadow_cascades[];
};

layout(std430, binding = 10) readonly buffer SpotShadowMapsBuffer
{
    ProjectedShadow u_spot_shadow_maps[];
};

layout(std430, binding = 11) readonly buffer AreaShadowMapsBuffer
{
    ProjectedShadow u_area_shadow_maps[];
};

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

vec3 tbx_reconstruct_world_position(vec2 uv, float depth, mat4 inverse_view_projection)
{
    vec4 clip_position = vec4((uv * 2.0) - 1.0, (depth * 2.0) - 1.0, 1.0);
    vec4 world_position = inverse_view_projection * clip_position;
    return world_position.xyz / max(world_position.w, 0.0001);
}

float tbx_get_distance_attenuation(float distance_squared, float range)
{
    float range_squared = max(range * range, 0.0001);
    float attenuation_mask = 1.0 - step(range_squared, distance_squared);
    float normalized_distance_squared = distance_squared / range_squared;
    float range_falloff = clamp(1.0 - normalized_distance_squared, 0.0, 1.0);
    return attenuation_mask * (range_falloff * range_falloff) / max(distance_squared, 0.25);
}

vec3 tbx_get_area_light_closest_point(AreaLight light, vec3 world_position)
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

float tbx_get_area_light_attenuation(AreaLight light, vec3 light_direction, float distance_squared)
{
    float base_attenuation = tbx_get_distance_attenuation(distance_squared, light.position_range.w);
    float front_face = max(dot(light.direction_half_width.xyz, -light_direction), 0.0);
    float emitter_extent = max(light.direction_half_width.w + light.radiance_half_height.w, 0.001);
    float softness = emitter_extent / (emitter_extent + sqrt(max(distance_squared, 0.0001)));
    return base_attenuation * front_face * (0.35 + (0.65 * softness));
}

float tbx_get_shadow_normal_offset(vec3 normal, vec3 light_direction, float normal_bias)
{
    return (1.0 - max(dot(normal, light_direction), 0.0)) * normal_bias;
}

float tbx_sample_projected_shadow(
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
    float current_depth = projected.z - depth_bias;
    float visibility = 0.0;
    for (int sample_y = -1; sample_y <= 1; ++sample_y)
        for (int sample_x = -1; sample_x <= 1; ++sample_x)
        {
            vec2 sample_uv = projected.xy + (vec2(sample_x, sample_y) * texel_size);
            float stored_depth = texture(
                shadow_texture,
                vec3(sample_uv, float(texture_layer))).r;
            visibility += step(current_depth, stored_depth);
        }

    return visibility / 9.0;
}

bool tbx_try_project_shadow_position(vec4 shadow_position, out vec3 projected)
{
    if (shadow_position.w <= 0.0)
        return false;

    projected = shadow_position.xyz / shadow_position.w;
    projected = (projected * 0.5) + 0.5;
    return projected.x > 0.0 && projected.x < 1.0 && projected.y > 0.0 && projected.y < 1.0
           && projected.z > 0.0 && projected.z < 1.0;
}

float tbx_sample_directional_shadow(
    DirectionalLight light,
    vec3 world_position,
    vec3 normal,
    vec3 light_direction)
{
    if (light.radiance_shadowed.w <= 0.5 || light.shadow_info.y <= 0)
        return 1.0;

    float view_depth = -(u_lighting_info.view_matrix * vec4(world_position, 1.0)).z;
    int cascade_offset = light.shadow_info.x;
    int cascade_count = light.shadow_info.y;

    for (int cascade_index = 0; cascade_index < cascade_count; ++cascade_index)
    {
        ShadowCascade cascade = u_directional_shadow_cascades[cascade_offset + cascade_index];
        float normal_offset = tbx_get_shadow_normal_offset(
            normal,
            light_direction,
            cascade.split_and_bias.y);
        vec4 shadow_position = cascade.light_view_projection
                               * vec4(world_position + (normal * normal_offset), 1.0);
        vec3 projected = vec3(0.0);
        bool is_inside_cascade = tbx_try_project_shadow_position(shadow_position, projected);
        float visibility = tbx_sample_projected_shadow(
            u_directional_shadows,
            shadow_position,
            cascade.texture_layer.x,
            cascade.split_and_bias.z);

        float split_depth = cascade.split_and_bias.x;
        float blend_distance = cascade.split_and_bias.w;
        bool is_last_cascade = cascade_index == cascade_count - 1;
        if (!is_last_cascade && view_depth >= split_depth - blend_distance && view_depth <= split_depth)
        {
            ShadowCascade next_cascade =
                u_directional_shadow_cascades[cascade_offset + cascade_index + 1];
            float next_normal_offset = tbx_get_shadow_normal_offset(
                normal,
                light_direction,
                next_cascade.split_and_bias.y);
            vec4 next_shadow_position = next_cascade.light_view_projection
                                        * vec4(world_position + (normal * next_normal_offset), 1.0);
            vec3 next_projected = vec3(0.0);
            bool is_inside_next_cascade =
                tbx_try_project_shadow_position(next_shadow_position, next_projected);
            float next_visibility = tbx_sample_projected_shadow(
                u_directional_shadows,
                next_shadow_position,
                next_cascade.texture_layer.x,
                next_cascade.split_and_bias.z);
            if (!is_inside_cascade)
            {
                if (is_inside_next_cascade)
                    return next_visibility;
                continue;
            }
            if (!is_inside_next_cascade)
                return visibility;
            float blend = clamp(
                (view_depth - (split_depth - blend_distance)) / max(blend_distance, 0.0001),
                0.0,
                1.0);
            return mix(visibility, next_visibility, blend);
        }

        if (view_depth <= split_depth || is_last_cascade)
        {
            if (is_inside_cascade)
                return visibility;
            continue;
        }
    }

    return 1.0;
}

float tbx_sample_projected_light_shadow(
    sampler2DArray shadow_texture,
    ProjectedShadow shadow_map,
    vec3 world_position,
    vec3 normal,
    vec3 light_direction)
{
    float normal_offset = tbx_get_shadow_normal_offset(
        normal,
        light_direction,
        shadow_map.planes_and_bias.z);
    vec4 shadow_position =
        shadow_map.light_view_projection * vec4(world_position + (normal * normal_offset), 1.0);
    return tbx_sample_projected_shadow(
        shadow_texture,
        shadow_position,
        shadow_map.texture_layer.x,
        shadow_map.planes_and_bias.w);
}

float tbx_sample_point_shadow(PointLight light, vec3 world_position, vec3 normal)
{
    if (light.shadow_info.x < 0)
        return 1.0;

    vec3 receiver_vector = world_position - light.position_range.xyz;
    float receiver_distance = length(receiver_vector);
    if (receiver_distance <= 0.0001 || receiver_distance >= light.position_range.w)
        return 1.0;

    vec3 receiver_direction = receiver_vector / receiver_distance;
    float facing = max(dot(normal, -receiver_direction), 0.0);
    float bias = (1.0 - facing) * light.radiance_shadow_bias.w;
    float current_depth = clamp(
        (receiver_distance - bias) / max(light.position_range.w, 0.0001),
        0.0,
        1.0);
    float stored_depth = texture(
        u_point_shadows,
        vec4(receiver_direction, float(light.shadow_info.x))).r;
    return step(current_depth, stored_depth);
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
    if (attenuation <= 0.0001 || n_dot_l <= 0.0001)
        return;

    vec3 half_vector = normalize(view_direction + light_direction);
    float specular_power = pow(max(dot(normal, half_vector), 0.0), shininess);
    vec3 light_energy = radiance * (n_dot_l * attenuation);
    diffuse_accumulation += albedo * light_energy;
    specular_accumulation += vec3(specular_strength * specular_power) * light_energy;
}

void main()
{
    float depth_sample = texture(u_depth, v_tex_coord).r;
    if (depth_sample >= 0.999999)
    {
        o_final_color = u_lighting_info.clear_color;
        return;
    }

    vec4 albedo_sample = texture(u_albedo, v_tex_coord);
    vec4 normal_sample = texture(u_normal, v_tex_coord);
    vec4 emissive_sample = texture(u_emissive, v_tex_coord);
    vec4 material_sample = texture(u_material, v_tex_coord);

    vec3 albedo = tbx_srgb_to_linear(albedo_sample.rgb);
    vec3 emissive = tbx_srgb_to_linear(emissive_sample.rgb);
    vec3 normal = normalize((normal_sample.xyz * 2.0) - 1.0);
    vec3 world_position =
        tbx_reconstruct_world_position(
            v_tex_coord,
            depth_sample,
            u_lighting_info.inverse_view_projection);
    vec3 view_delta = u_lighting_info.camera_position.xyz - world_position;
    vec3 view_direction = normalize(view_delta + vec3(0.0, 0.0, step(length(view_delta), 0.0001)));

    float specular_strength = clamp(material_sample.r, 0.0, 1.0);
    float shininess = clamp(material_sample.g, 1.0, 256.0);
    float exposure = max(emissive_sample.a, 0.0001);

    vec3 diffuse_accumulation = vec3(0.0);
    vec3 specular_accumulation = vec3(0.0);
    float hemisphere_factor = clamp((normal.y * 0.5) + 0.5, 0.0, 1.0);
    vec3 hemisphere_ambient =
        mix(vec3(0.05, 0.045, 0.04), vec3(0.17, 0.19, 0.23), hemisphere_factor);
    float fresnel = pow(1.0 - max(dot(normal, view_direction), 0.0), 4.0);
    vec3 ambient_accumulation = albedo * hemisphere_ambient;
    vec3 fresnel_accumulation = vec3(specular_strength * fresnel * 0.08);

    uint tile_size = uint(max(u_lighting_info.tile_info.x, 1));
    uint tile_count_x = uint(max(u_lighting_info.tile_info.y, 1));
    uint tile_count_y = uint(max(u_lighting_info.tile_info.z, 1));
    uint tile_x = min(uint(gl_FragCoord.x) / tile_size, tile_count_x - 1U);
    uint tile_y = min(uint(gl_FragCoord.y) / tile_size, tile_count_y - 1U);
    uint tile_index = tile_y * tile_count_x + tile_x;
    TileLightSpan tile_light_span = u_tile_light_spans[tile_index];

    for (int light_index = 0; light_index < u_lighting_info.light_counts.x; ++light_index)
    {
        DirectionalLight light = u_lighting_info.directional_lights[light_index];
        vec3 light_direction = normalize(-light.direction_ambient.xyz);
        vec3 light_radiance = light.radiance_shadowed.rgb;
        float shadow_visibility =
            tbx_sample_directional_shadow(light, world_position, normal, light_direction);
        ambient_accumulation += albedo * light_radiance * light.direction_ambient.w;
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light_radiance,
            shadow_visibility,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.point_and_spot.y;
         ++tile_light_index)
    {
        PointLight light =
            u_point_lights[u_tile_point_light_indices[tile_light_span.point_and_spot.x + tile_light_index]];
        vec3 to_light = light.position_range.xyz - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float attenuation = tbx_get_distance_attenuation(distance_squared, light.position_range.w);
        float shadow_visibility = tbx_sample_point_shadow(light, world_position, normal);
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance_shadow_bias.rgb,
            attenuation * shadow_visibility,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.point_and_spot.w;
         ++tile_light_index)
    {
        SpotLight light =
            u_spot_lights[u_tile_spot_light_indices[tile_light_span.point_and_spot.z + tile_light_index]];
        vec3 to_light = light.position_range.xyz - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float attenuation = tbx_get_distance_attenuation(distance_squared, light.position_range.w);
        float spot_cos = dot(light.direction_inner_cos.xyz, -light_direction);
        float cone = clamp(
            (spot_cos - light.radiance_outer_cos.w)
                / max(light.direction_inner_cos.w - light.radiance_outer_cos.w, 0.0001),
            0.0,
            1.0);
        float cone_mask = step(light.radiance_outer_cos.w, spot_cos);
        float shadow_visibility = 1.0;
        if (light.shadow_info.x >= 0)
            shadow_visibility = tbx_sample_projected_light_shadow(
                u_spot_shadows,
                u_spot_shadow_maps[light.shadow_info.x],
                world_position,
                normal,
                light_direction);
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance_outer_cos.rgb,
            attenuation * cone * cone * cone_mask * shadow_visibility,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.area.y; ++tile_light_index)
    {
        AreaLight light =
            u_area_lights[u_tile_area_light_indices[tile_light_span.area.x + tile_light_index]];
        vec3 closest_point = tbx_get_area_light_closest_point(light, world_position);
        vec3 to_light = closest_point - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = mix(
            -light.direction_half_width.xyz,
            to_light * inversesqrt(max(distance_squared, 0.0001)),
            step(0.0001, distance_squared));
        float attenuation = tbx_get_area_light_attenuation(light, light_direction, distance_squared);
        float shadow_visibility = 1.0;
        if (light.shadow_info.x >= 0)
            shadow_visibility = tbx_sample_projected_light_shadow(
                u_area_shadows,
                u_area_shadow_maps[light.shadow_info.x],
                world_position,
                normal,
                light_direction);
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance_half_height.rgb,
            attenuation * shadow_visibility,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    vec3 hdr_lighting_color =
        ambient_accumulation + diffuse_accumulation + specular_accumulation
        + fresnel_accumulation;
    hdr_lighting_color += emissive;
    hdr_lighting_color *= exposure;

    vec3 presented_color = tbx_linear_to_srgb(tbx_tonemap_aces(hdr_lighting_color));
    o_final_color = vec4(clamp(presented_color, 0.0, 1.0), albedo_sample.a);
}
