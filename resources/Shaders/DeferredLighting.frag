#version 450 core
#include Globals.glsl

in vec2 v_tex_coord;

layout(location = 0) out vec4 o_color;

struct DirectionalLight
{
    vec3 direction;
    float ambient_intensity;
    vec3 radiance;
    float padding;
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

const int MAX_DIRECTIONAL_LIGHTS = 4;

uniform sampler2D u_gbuffer_albedo;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_emissive;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_gbuffer_depth;

uniform vec3 u_camera_position;
uniform vec4 u_clear_color;
uniform mat4 u_inverse_view_projection;
uniform int u_tile_size;
uniform int u_tile_count_x;
uniform int u_tile_count_y;

uniform int u_directional_light_count;
uniform DirectionalLight u_directional_lights[MAX_DIRECTIONAL_LIGHTS];

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

vec3 reconstruct_world_position(vec2 uv, float depth)
{
    vec4 clip_position = vec4((uv * 2.0) - 1.0, (depth * 2.0) - 1.0, 1.0);
    vec4 world_position = u_inverse_view_projection * clip_position;
    return world_position.xyz / max(world_position.w, 0.0001);
}

float get_distance_attenuation(float distance_squared, float range)
{
    float range_squared = max(range * range, 0.0001);
    if (distance_squared >= range_squared)
        return 0.0;

    float normalized_distance_squared = distance_squared / range_squared;
    float range_falloff = clamp(1.0 - normalized_distance_squared, 0.0, 1.0);
    return (range_falloff * range_falloff) / max(distance_squared, 0.25);
}

vec3 get_area_light_closest_point(AreaLight light, vec3 world_position)
{
    vec3 relative_to_center = world_position - light.position;
    float local_x = clamp(
        dot(relative_to_center, light.right),
        -light.half_width,
        light.half_width);
    float local_y = clamp(
        dot(relative_to_center, light.up),
        -light.half_height,
        light.half_height);
    return light.position + (light.right * local_x) + (light.up * local_y);
}

float get_area_light_attenuation(
    AreaLight light,
    vec3 light_direction,
    float distance_squared)
{
    float base_attenuation = get_distance_attenuation(distance_squared, light.range);
    float front_face = max(dot(light.direction, -light_direction), 0.0);
    float emitter_extent = max(light.half_width + light.half_height, 0.001);
    float softness = emitter_extent / (emitter_extent + sqrt(max(distance_squared, 0.0001)));
    return base_attenuation * front_face * (0.35 + (0.65 * softness));
}

void accumulate_light(
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
    if (n_dot_l <= 0.0 || attenuation <= 0.0)
        return;

    vec3 half_vector = normalize(view_direction + light_direction);
    float specular_power = pow(max(dot(normal, half_vector), 0.0), shininess);
    vec3 light_energy = radiance * (n_dot_l * attenuation);
    diffuse_accumulation += albedo * light_energy;
    specular_accumulation += vec3(specular_strength * specular_power) * light_energy;
}

void main()
{
    float depth = texture(u_gbuffer_depth, v_tex_coord).r;
    if (depth >= 0.999999)
    {
        o_color = u_clear_color;
        return;
    }

    vec4 albedo_sample = texture(u_gbuffer_albedo, v_tex_coord);
    vec4 normal_sample = texture(u_gbuffer_normal, v_tex_coord);
    vec4 emissive_sample = texture(u_gbuffer_emissive, v_tex_coord);
    vec4 material_sample = texture(u_gbuffer_material, v_tex_coord);

    vec3 albedo = tbx_srgb_to_linear(albedo_sample.rgb);
    vec3 emissive = tbx_srgb_to_linear(emissive_sample.rgb);
    vec3 normal = normalize((normal_sample.xyz * 2.0) - 1.0);
    vec3 world_position = reconstruct_world_position(v_tex_coord, depth);
    vec3 view_delta = u_camera_position - world_position;
    vec3 view_direction =
        length(view_delta) > 0.0001 ? normalize(view_delta) : vec3(0.0, 0.0, 1.0);

    float specular_strength = clamp(material_sample.r, 0.0, 1.0);
    float shininess = clamp(material_sample.g, 1.0, 256.0);
    float occlusion = clamp(material_sample.b, 0.0, 1.0);
    float exposure = max(emissive_sample.a, 0.0);

    vec3 diffuse_accumulation = vec3(0.0);
    vec3 specular_accumulation = vec3(0.0);
    float hemisphere_factor = clamp((normal.y * 0.5) + 0.5, 0.0, 1.0);
    vec3 hemisphere_ambient =
        mix(vec3(0.05, 0.045, 0.04), vec3(0.17, 0.19, 0.23), hemisphere_factor) * occlusion;
    float fresnel = pow(1.0 - max(dot(normal, view_direction), 0.0), 4.0);
    vec3 ambient_accumulation = albedo * hemisphere_ambient;
    vec3 fresnel_accumulation = vec3(specular_strength * fresnel * 0.08);
    uint tile_x = min(uint(gl_FragCoord.x) / uint(max(u_tile_size, 1)), uint(max(u_tile_count_x - 1, 0)));
    uint tile_y = min(uint(gl_FragCoord.y) / uint(max(u_tile_size, 1)), uint(max(u_tile_count_y - 1, 0)));
    uint tile_index = tile_y * uint(max(u_tile_count_x, 1)) + tile_x;
    TileLightSpan tile_light_span = u_tile_light_spans[tile_index];

    for (int light_index = 0; light_index < u_directional_light_count; ++light_index)
    {
        DirectionalLight light = u_directional_lights[light_index];
        ambient_accumulation += albedo * light.radiance * (light.ambient_intensity * occlusion);
        accumulate_light(
            albedo,
            normal,
            view_direction,
            normalize(-light.direction),
            light.radiance,
            1.0,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.point_count; ++tile_light_index)
    {
        PointLight light =
            u_point_lights[u_tile_point_light_indices[tile_light_span.point_offset + tile_light_index]];
        vec3 to_light = light.position - world_position;
        float distance_squared = dot(to_light, to_light);
        if (distance_squared >= light.range * light.range)
            continue;

        float inverse_distance = inversesqrt(max(distance_squared, 0.0001));
        vec3 light_direction = to_light * inverse_distance;
        float attenuation = get_distance_attenuation(distance_squared, light.range);
        accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance,
            attenuation,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.spot_count; ++tile_light_index)
    {
        SpotLight light =
            u_spot_lights[u_tile_spot_light_indices[tile_light_span.spot_offset + tile_light_index]];
        vec3 to_light = light.position - world_position;
        float distance_squared = dot(to_light, to_light);
        if (distance_squared >= light.range * light.range)
            continue;

        float inverse_distance = inversesqrt(max(distance_squared, 0.0001));
        vec3 light_direction = to_light * inverse_distance;
        float attenuation = get_distance_attenuation(distance_squared, light.range);
        float spot_cos = dot(light.direction, -light_direction);
        if (spot_cos <= light.outer_cos)
            continue;

        float cone = clamp(
            (spot_cos - light.outer_cos) / max(light.inner_cos - light.outer_cos, 0.0001),
            0.0,
            1.0);
        accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance,
            attenuation * cone * cone,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.area_count; ++tile_light_index)
    {
        AreaLight light =
            u_area_lights[u_tile_area_light_indices[tile_light_span.area_offset + tile_light_index]];
        vec3 closest_point = get_area_light_closest_point(light, world_position);
        vec3 to_light = closest_point - world_position;
        float distance_squared = dot(to_light, to_light);
        if (distance_squared >= light.range * light.range)
            continue;

        vec3 light_direction =
            distance_squared > 0.0001 ? to_light * inversesqrt(distance_squared) : -light.direction;
        float attenuation = get_area_light_attenuation(light, light_direction, distance_squared);
        accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance,
            attenuation,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    vec3 final_color =
        ambient_accumulation + diffuse_accumulation + specular_accumulation + fresnel_accumulation;
    final_color += emissive;
    final_color *= exposure;

    vec3 display_color = tbx_linear_to_srgb(final_color);
    float dither = tbx_interleaved_gradient_noise(gl_FragCoord.xy) - 0.5;
    display_color += vec3(dither / 255.0);
    o_color = vec4(clamp(display_color, 0.0, 1.0), albedo_sample.a);
}
