#version 450 core

in vec2 v_tex_coord;

layout(location = 0) out vec4 o_color;

uniform sampler2D u_gbuffer_albedo;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_emissive;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_gbuffer_depth;
uniform sampler2DArray u_shadow_map;

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

struct ShadowCascade
{
    mat4 light_view_projection;
    float split_depth;
    float normal_bias;
    float depth_bias;
    float blend_distance;
};

struct ShadowInfo
{
    ShadowCascade cascades[3];
    vec4 cascade_splits;
    vec4 metadata;
};

layout(std140, binding = 7) uniform LightingInfoBuffer
{
    LightingInfo u_lighting_info;
};

layout(std140, binding = 8) uniform ShadowInfoBuffer
{
    ShadowInfo u_shadow_info;
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
    float active = 1.0 - step(range_squared, distance_squared);
    float normalized_distance_squared = distance_squared / range_squared;
    float range_falloff = clamp(1.0 - normalized_distance_squared, 0.0, 1.0);
    return active * (range_falloff * range_falloff) / max(distance_squared, 0.25);
}

vec3 tbx_get_area_light_closest_point(AreaLight light, vec3 world_position)
{
    vec3 relative_to_center = world_position - light.position;
    float local_x = clamp(dot(relative_to_center, light.right), -light.half_width, light.half_width);
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

int tbx_get_shadow_cascade_index(float view_depth, ShadowInfo shadow_info)
{
    float cascade_index = 0.0;
    cascade_index += step(shadow_info.cascade_splits.x, view_depth);
    cascade_index += step(shadow_info.cascade_splits.y, view_depth);
    return int(min(cascade_index, 2.0));
}

float tbx_sample_shadow_cascade(
    sampler2DArray shadow_map,
    ShadowInfo shadow_info,
    int cascade_index,
    vec3 normal,
    vec3 light_direction,
    vec3 world_position)
{
    ShadowCascade cascade = shadow_info.cascades[cascade_index];
    vec4 shadow_position = cascade.light_view_projection * vec4(world_position, 1.0);
    vec3 projected = shadow_position.xyz / max(shadow_position.w, 0.0001);
    projected = (projected * 0.5) + 0.5;
    float inside = step(0.0, projected.x) * step(0.0, projected.y) * step(0.0, projected.z)
                   * step(projected.x, 1.0) * step(projected.y, 1.0) * step(projected.z, 1.0);
    if (inside <= 0.0)
        return 1.0;

    float slope_scale = 1.0 - max(dot(normal, light_direction), 0.0);
    float bias = cascade.depth_bias + (cascade.normal_bias * slope_scale);
    float current_depth = projected.z - bias;
    ivec2 shadow_size = textureSize(shadow_map, 0).xy;
    vec2 texel_size = 1.0 / vec2(max(shadow_size.x, 1), max(shadow_size.y, 1));
    float kernel_radius = max(u_shadow_info.metadata.z, 0.35);
    vec2 kernel[9] = vec2[](
        vec2(0.0, 0.0),
        vec2(-1.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, -1.0),
        vec2(0.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(1.0, -1.0),
        vec2(-1.0, 1.0),
        vec2(1.0, 1.0));

    float visibility = 0.0;
    for (int sample_index = 0; sample_index < 9; ++sample_index)
    {
        vec2 offset = kernel[sample_index] * texel_size * kernel_radius;
        float sampled_depth = texture(shadow_map, vec3(projected.xy + offset, float(cascade_index))).r;
        visibility += step(current_depth, sampled_depth);
    }

    return visibility / 9.0;
}

float tbx_get_shadow_visibility(
    sampler2DArray shadow_map,
    ShadowInfo shadow_info,
    vec3 normal,
    vec3 light_direction,
    vec3 world_position,
    float view_depth)
{
    if (shadow_info.metadata.x <= 0.0)
        return 1.0;

    int cascade_index = tbx_get_shadow_cascade_index(view_depth, shadow_info);
    float primary_visibility = tbx_sample_shadow_cascade(
        shadow_map,
        shadow_info,
        cascade_index,
        normal,
        light_direction,
        world_position);
    if (cascade_index >= 2)
        return primary_visibility;

    float split_depth = shadow_info.cascades[cascade_index].split_depth;
    float blend_distance = max(shadow_info.cascades[cascade_index].blend_distance, 0.0001);
    float blend_weight = clamp((view_depth - (split_depth - blend_distance)) / blend_distance, 0.0, 1.0);
    float next_visibility = tbx_sample_shadow_cascade(
        shadow_map,
        shadow_info,
        cascade_index + 1,
        normal,
        light_direction,
        world_position);
    return mix(primary_visibility, next_visibility, blend_weight);
}

void main()
{
    float depth = texture(u_gbuffer_depth, v_tex_coord).r;
    if (depth >= 0.999999)
    {
        o_color = u_lighting_info.clear_color;
        return;
    }

    vec4 albedo_sample = texture(u_gbuffer_albedo, v_tex_coord);
    vec4 normal_sample = texture(u_gbuffer_normal, v_tex_coord);
    vec4 emissive_sample = texture(u_gbuffer_emissive, v_tex_coord);
    vec4 material_sample = texture(u_gbuffer_material, v_tex_coord);

    vec3 albedo = tbx_srgb_to_linear(albedo_sample.rgb);
    vec3 emissive = tbx_srgb_to_linear(emissive_sample.rgb);
    vec3 normal = normalize((normal_sample.xyz * 2.0) - 1.0);
    vec3 world_position = tbx_reconstruct_world_position(v_tex_coord, depth, u_lighting_info.inverse_view_projection);
    vec3 view_delta = u_lighting_info.camera_position.xyz - world_position;
    vec3 view_direction = normalize(view_delta + vec3(0.0, 0.0, step(length(view_delta), 0.0001)));
    float view_depth = length(view_delta);

    float specular_strength = clamp(material_sample.r, 0.0, 1.0);
    float shininess = clamp(material_sample.g, 1.0, 256.0);
    float occlusion = clamp(material_sample.b, 0.0, 1.0);
    float exposure = max(emissive_sample.a, 0.0001);

    vec3 diffuse_accumulation = vec3(0.0);
    vec3 specular_accumulation = vec3(0.0);
    float hemisphere_factor = clamp((normal.y * 0.5) + 0.5, 0.0, 1.0);
    vec3 hemisphere_ambient = mix(vec3(0.05, 0.045, 0.04), vec3(0.17, 0.19, 0.23), hemisphere_factor) * occlusion;
    float fresnel = pow(1.0 - max(dot(normal, view_direction), 0.0), 4.0);
    vec3 ambient_accumulation = albedo * hemisphere_ambient;
    vec3 fresnel_accumulation = vec3(specular_strength * fresnel * 0.08);
    uint tile_x = min(uint(gl_FragCoord.x) / uint(max(u_lighting_info.tile_info.x, 1)), uint(max(u_lighting_info.tile_info.y - 1, 0)));
    uint tile_y = min(uint(gl_FragCoord.y) / uint(max(u_lighting_info.tile_info.x, 1)), uint(max(u_lighting_info.tile_info.z - 1, 0)));
    uint tile_index = tile_y * uint(max(u_lighting_info.tile_info.y, 1)) + tile_x;
    TileLightSpan tile_light_span = u_tile_light_spans[tile_index];

    for (int light_index = 0; light_index < u_lighting_info.light_counts.x; ++light_index)
    {
        DirectionalLight light = u_lighting_info.directional_lights[light_index];
        vec3 light_direction = normalize(-light.direction);
        float shadow_visibility = mix(1.0, tbx_get_shadow_visibility(u_shadow_map, u_shadow_info, normal, light_direction, world_position, view_depth), clamp(light.casts_shadows, 0.0, 1.0));
        ambient_accumulation += albedo * light.radiance * (light.ambient_intensity * occlusion);
        tbx_accumulate_light(albedo, normal, view_direction, light_direction, light.radiance, shadow_visibility, specular_strength, shininess, diffuse_accumulation, specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.point_count; ++tile_light_index)
    {
        PointLight light = u_point_lights[u_tile_point_light_indices[tile_light_span.point_offset + tile_light_index]];
        vec3 to_light = light.position - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float attenuation = tbx_get_distance_attenuation(distance_squared, light.range);
        tbx_accumulate_light(albedo, normal, view_direction, light_direction, light.radiance, attenuation, specular_strength, shininess, diffuse_accumulation, specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.spot_count; ++tile_light_index)
    {
        SpotLight light = u_spot_lights[u_tile_spot_light_indices[tile_light_span.spot_offset + tile_light_index]];
        vec3 to_light = light.position - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float attenuation = tbx_get_distance_attenuation(distance_squared, light.range);
        float spot_cos = dot(light.direction, -light_direction);
        float cone = clamp((spot_cos - light.outer_cos) / max(light.inner_cos - light.outer_cos, 0.0001), 0.0, 1.0);
        float cone_mask = step(light.outer_cos, spot_cos);
        tbx_accumulate_light(albedo, normal, view_direction, light_direction, light.radiance, attenuation * cone * cone * cone_mask, specular_strength, shininess, diffuse_accumulation, specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.area_count; ++tile_light_index)
    {
        AreaLight light = u_area_lights[u_tile_area_light_indices[tile_light_span.area_offset + tile_light_index]];
        vec3 closest_point = tbx_get_area_light_closest_point(light, world_position);
        vec3 to_light = closest_point - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = mix(-light.direction, to_light * inversesqrt(max(distance_squared, 0.0001)), step(0.0001, distance_squared));
        float attenuation = tbx_get_area_light_attenuation(light, light_direction, distance_squared);
        tbx_accumulate_light(albedo, normal, view_direction, light_direction, light.radiance, attenuation, specular_strength, shininess, diffuse_accumulation, specular_accumulation);
    }

    vec3 final_color = ambient_accumulation + diffuse_accumulation + specular_accumulation + fresnel_accumulation;
    final_color += emissive;
    final_color *= exposure;

    vec3 display_color = tbx_linear_to_srgb(tbx_tonemap_aces(final_color));
    float dither = tbx_interleaved_gradient_noise(gl_FragCoord.xy) - 0.5;
    display_color += vec3(dither / 255.0);
    o_color = vec4(clamp(display_color, 0.0, 1.0), albedo_sample.a);
}
