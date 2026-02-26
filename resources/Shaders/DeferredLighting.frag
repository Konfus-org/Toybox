#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec2 v_tex_coord;

uniform sampler2D u_gbuffer_albedo_spec;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_scene_depth;
uniform sampler2DShadow u_shadow_maps[24];

uniform vec3 u_camera_position = vec3(0.0);
uniform vec3 u_camera_forward = vec3(0.0, 0.0, -1.0);
uniform mat4 u_inverse_view_projection = mat4(1.0);
uniform mat4 u_light_view_projection_matrices[24];
uniform float u_cascade_splits[4] = float[4](10.0, 25.0, 60.0, 1000.0);
uniform int u_shadow_map_count = 0;
uniform float u_shadow_softness = 1.75;
uniform float u_exposure = 0.6;

uniform vec2 u_render_resolution = vec2(1.0, 1.0);
uniform int u_tile_size = 16;
uniform int u_tile_count_x = 0;
uniform int u_tile_count_y = 0;
uniform int u_packed_light_count = 0;
uniform bool u_compute_culling_enabled = true;
uniform bool u_include_local_lights = false;

const int LIGHT_TYPE_DIRECTIONAL = 0;
const int LIGHT_TYPE_POINT = 1;
const int LIGHT_TYPE_SPOT = 2;
const int LIGHT_TYPE_AREA = 3;
const int DIRECTIONAL_SHADOW_CASCADE_COUNT = 2;
const int POINT_SHADOW_FACE_COUNT = 6;
const int DIRECTIONAL_PCF_KERNEL_RADIUS = 2;
const int LOCAL_PCF_KERNEL_RADIUS = 1;
const float DIRECTIONAL_CASCADE_TRANSITION_RATIO = 0.1;
const float OUTPUT_DITHER_STRENGTH = 1.0 / 255.0;

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

layout(std430, binding = 1) readonly buffer TileHeaderBuffer
{
    uvec4 tile_headers[];
};

layout(std430, binding = 2) readonly buffer TileLightIndexBuffer
{
    uint tile_light_indices[];
};

float calc_distance_attenuation(float distance_to_light, float range)
{
    if (range <= 0.0)
        return 0.0;

    float normalized = clamp(distance_to_light / range, 0.0, 1.0);
    float attenuation_curve = 1.0 - normalized * normalized;
    return attenuation_curve * attenuation_curve / max(distance_to_light * distance_to_light, 0.01);
}

vec3 reconstruct_world_position(vec2 uv, float depth)
{
    vec4 clip_space = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = u_inverse_view_projection * clip_space;
    float w = world.w;
    if (abs(w) <= 1e-7)
        return vec3(0.0);

    return world.xyz / w;
}

vec3 safe_normalize(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    if (length_squared <= 1e-10)
        return fallback;

    return value * inversesqrt(length_squared);
}

void build_light_basis(vec3 axis, out vec3 tangent, out vec3 bitangent)
{
    vec3 fallback_up = abs(axis.y) > 0.95 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    tangent = safe_normalize(cross(fallback_up, axis), vec3(1.0, 0.0, 0.0));
    bitangent = safe_normalize(cross(axis, tangent), vec3(0.0, 0.0, 1.0));
}

float interleaved_gradient_noise(vec2 pixel_coord)
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(pixel_coord, magic.xy)));
}

vec3 dither_srgb_output(vec3 color_srgb)
{
    float noise = interleaved_gradient_noise(gl_FragCoord.xy);
    float dither = (noise - 0.5) * OUTPUT_DITHER_STRENGTH;
    return clamp(color_srgb + vec3(dither), vec3(0.0), vec3(1.0));
}

int select_point_shadow_face_index(vec3 light_to_fragment)
{
    vec3 abs_vector = abs(light_to_fragment);
    if (abs_vector.x >= abs_vector.y && abs_vector.x >= abs_vector.z)
        return light_to_fragment.x >= 0.0 ? 0 : 1;

    if (abs_vector.y >= abs_vector.x && abs_vector.y >= abs_vector.z)
        return light_to_fragment.y >= 0.0 ? 2 : 3;

    return light_to_fragment.z >= 0.0 ? 4 : 5;
}

float sample_shadow_visibility(
    int shadow_map_index,
    vec2 shadow_uv,
    float current_depth,
    float bias,
    float softness_scale,
    int kernel_radius)
{
    if (shadow_map_index < 0 || shadow_map_index >= u_shadow_map_count)
        return 1.0;

    float radius_texels =
        clamp(u_shadow_softness * softness_scale * max(float(kernel_radius), 1.0), 0.0, 3.0);
    float ref = clamp(current_depth - bias, 0.0, 1.0);
    float center = texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv, ref));
    if (radius_texels <= 0.001)
        return center;

    vec2 texel = 1.0 / vec2(textureSize(u_shadow_maps[shadow_map_index], 0));
    vec2 r = texel * radius_texels;
    float sum = 0.0;
    float weight_sum = 0.0;
    const float center_weight = 4.0;
    const float axis_weight = 2.0;
    const float corner_weight = 1.0;

    sum += center * center_weight;
    weight_sum += center_weight;

    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(-r.x, 0.0), ref))
        * axis_weight;
    weight_sum += axis_weight;
    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(r.x, 0.0), ref))
        * axis_weight;
    weight_sum += axis_weight;
    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(0.0, -r.y), ref))
        * axis_weight;
    weight_sum += axis_weight;
    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(0.0, r.y), ref))
        * axis_weight;
    weight_sum += axis_weight;

    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(-r.x, -r.y), ref))
        * corner_weight;
    weight_sum += corner_weight;
    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(r.x, -r.y), ref))
        * corner_weight;
    weight_sum += corner_weight;
    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(-r.x, r.y), ref))
        * corner_weight;
    weight_sum += corner_weight;
    sum += texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv + vec2(r.x, r.y), ref))
        * corner_weight;
    weight_sum += corner_weight;

    return sum / max(weight_sum, 0.0001);
}

bool try_sample_projected_shadow_visibility(
    int shadow_map_index,
    vec4 light_space_position,
    float bias,
    float softness_scale,
    int kernel_radius,
    out float out_visibility)
{
    out_visibility = 1.0;
    if (shadow_map_index < 0 || shadow_map_index >= u_shadow_map_count)
        return false;
    if (light_space_position.w <= 1e-5)
        return false;

    vec3 projected = light_space_position.xyz / light_space_position.w;
    if (projected.x < -1.0 || projected.x > 1.0 || projected.y < -1.0 || projected.y > 1.0
        || projected.z < -1.0 || projected.z > 1.0)
    {
        return false;
    }

    vec2 shadow_uv = projected.xy * 0.5 + 0.5;
    float current_depth = projected.z * 0.5 + 0.5;
    out_visibility = sample_shadow_visibility(
        shadow_map_index,
        shadow_uv,
        current_depth,
        bias,
        softness_scale,
        kernel_radius);
    return true;
}

bool try_sample_light_shadow_visibility(
    int shadow_map_index,
    vec3 world_position,
    vec3 normal,
    vec3 light_direction,
    float bias_scale,
    float min_bias,
    float normal_offset_base,
    float normal_offset_slope,
    float softness_scale,
    int kernel_radius,
    out float out_visibility)
{
    out_visibility = 1.0;
    if (shadow_map_index < 0 || shadow_map_index >= u_shadow_map_count)
        return false;

    float ndotl = clamp(dot(normal, light_direction), 0.0, 1.0);
    float bias = max(bias_scale * (1.0 - ndotl), min_bias);
    float slope_term = 1.0 - clamp(ndotl, 0.0, 1.0);
    float offset = max(normal_offset_base + (slope_term * normal_offset_slope), 0.0);
    vec3 shadow_position = world_position + (normal * offset);
    vec4 light_space_position =
        u_light_view_projection_matrices[shadow_map_index] * vec4(shadow_position, 1.0);

    return try_sample_projected_shadow_visibility(
        shadow_map_index,
        light_space_position,
        bias,
        softness_scale,
        kernel_radius,
        out_visibility);
}

float sample_light_shadow_visibility(
    int shadow_map_index,
    vec3 world_position,
    vec3 normal,
    vec3 light_direction,
    float bias_scale,
    float min_bias,
    float normal_offset_base,
    float normal_offset_slope,
    float softness_scale,
    int kernel_radius)
{
    float visibility = 1.0;
    try_sample_light_shadow_visibility(
        shadow_map_index,
        world_position,
        normal,
        light_direction,
        bias_scale,
        min_bias,
        normal_offset_base,
        normal_offset_slope,
        softness_scale,
        kernel_radius,
        visibility);
    return visibility;
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

vec3 evaluate_directional_light(
    PackedLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 light_direction = safe_normalize(light.direction_inner_cos.xyz, vec3(0.0, 0.0, -1.0));
    vec3 light_color = light.color_intensity.xyz;
    float light_intensity = light.color_intensity.w;
    float ambient_intensity = max(light.area_outer_ambient.w, 0.0);
    int base_shadow_map_index = light.metadata.y;

    vec3 ambient = albedo * light_color * ambient_intensity;
    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light_color,
        light_intensity,
        specular_strength,
        shininess);

    float shadow_visibility = 1.0;
    if (base_shadow_map_index >= 0)
    {
        const int cascade0_shadow_map_index = base_shadow_map_index;
        const int cascade1_shadow_map_index = base_shadow_map_index + 1;
        const bool has_second_cascade =
            cascade1_shadow_map_index < u_shadow_map_count && DIRECTIONAL_SHADOW_CASCADE_COUNT > 1;

        const float cascade0_softness = 0.85;
        const float cascade1_softness = 1.15;
        vec3 camera_forward = safe_normalize(u_camera_forward, vec3(0.0, 0.0, -1.0));
        const float view_depth = max(dot(world_position - u_camera_position, camera_forward), 0.0);
        float pcf_visibility = 1.0;

        if (!has_second_cascade)
        {
            pcf_visibility = sample_light_shadow_visibility(
                cascade0_shadow_map_index,
                world_position,
                normal,
                light_direction,
                0.0008,
                0.00015,
                0.0025,
                0.012,
                cascade0_softness,
                DIRECTIONAL_PCF_KERNEL_RADIUS);
        }
        else
        {
            const float split_depth = max(u_cascade_splits[0], 0.0001);
            const float blend_width = max(split_depth * DIRECTIONAL_CASCADE_TRANSITION_RATIO, 0.0001);
            const float blend_half_width = blend_width * 0.5;
            const float blend_start = max(0.0, split_depth - blend_half_width);
            const float blend_end = split_depth + blend_half_width;

            if (view_depth <= blend_start)
            {
                pcf_visibility = sample_light_shadow_visibility(
                    cascade0_shadow_map_index,
                    world_position,
                    normal,
                    light_direction,
                    0.0008,
                    0.00015,
                    0.0025,
                    0.012,
                    cascade0_softness,
                    DIRECTIONAL_PCF_KERNEL_RADIUS);
            }
            else if (view_depth >= blend_end)
            {
                pcf_visibility = sample_light_shadow_visibility(
                    cascade1_shadow_map_index,
                    world_position,
                    normal,
                    light_direction,
                    0.0014,
                    0.00035,
                    0.0040,
                    0.018,
                    cascade1_softness,
                    DIRECTIONAL_PCF_KERNEL_RADIUS);
            }
            else
            {
                const float cascade_blend_t = smoothstep(blend_start, blend_end, view_depth);
                float cascade0_visibility = 1.0;
                float cascade1_visibility = 1.0;

                const bool has_cascade0_visibility = try_sample_light_shadow_visibility(
                    cascade0_shadow_map_index,
                    world_position,
                    normal,
                    light_direction,
                    mix(0.0008, 0.0014, cascade_blend_t),
                    mix(0.00015, 0.00035, cascade_blend_t),
                    mix(0.0025, 0.0040, cascade_blend_t),
                    mix(0.012, 0.018, cascade_blend_t),
                    mix(cascade0_softness, cascade1_softness, cascade_blend_t),
                    DIRECTIONAL_PCF_KERNEL_RADIUS,
                    cascade0_visibility);

                const bool has_cascade1_visibility = try_sample_light_shadow_visibility(
                    cascade1_shadow_map_index,
                    world_position,
                    normal,
                    light_direction,
                    mix(0.0008, 0.0014, cascade_blend_t),
                    mix(0.00015, 0.00035, cascade_blend_t),
                    mix(0.0025, 0.0040, cascade_blend_t),
                    mix(0.012, 0.018, cascade_blend_t),
                    mix(cascade0_softness, cascade1_softness, cascade_blend_t),
                    DIRECTIONAL_PCF_KERNEL_RADIUS,
                    cascade1_visibility);

                if (has_cascade0_visibility && has_cascade1_visibility)
                    pcf_visibility = mix(cascade0_visibility, cascade1_visibility, cascade_blend_t);
                else if (has_cascade0_visibility)
                    pcf_visibility = cascade0_visibility;
                else if (has_cascade1_visibility)
                    pcf_visibility = cascade1_visibility;
            }
        }

        shadow_visibility = mix(0.15, 1.0, pcf_visibility);
    }

    return ambient + (direct * shadow_visibility);
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
    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light.color_intensity.xyz,
        light.color_intensity.w,
        specular_strength,
        shininess);

    float shadow_visibility = 1.0;
    int base_shadow_map_index = light.metadata.y;
    if (base_shadow_map_index >= 0
        && base_shadow_map_index + (POINT_SHADOW_FACE_COUNT - 1) < u_shadow_map_count)
    {
        vec3 light_to_fragment = world_position - light.position_range.xyz;
        int face_index = select_point_shadow_face_index(light_to_fragment);
        int shadow_map_index = base_shadow_map_index + face_index;
        float pcf_visibility = sample_light_shadow_visibility(
            shadow_map_index,
            world_position,
            normal,
            light_direction,
            0.0005,
            0.0001,
            0.002,
            0.008,
            0.8,
            LOCAL_PCF_KERNEL_RADIUS);
        shadow_visibility = mix(0.2, 1.0, pcf_visibility);
    }

    return direct * attenuation * shadow_visibility;
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

    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light.color_intensity.xyz,
        light.color_intensity.w,
        specular_strength,
        shininess);

    float shadow_visibility = 1.0;
    int shadow_map_index = light.metadata.y;
    if (shadow_map_index >= 0)
    {
        float pcf_visibility = sample_light_shadow_visibility(
            shadow_map_index,
            world_position,
            normal,
            light_direction,
            0.0003,
            0.00005,
            0.0015,
            0.007,
            0.85,
            LOCAL_PCF_KERNEL_RADIUS);
        shadow_visibility = mix(0.2, 1.0, pcf_visibility);
    }

    return direct * attenuation * shadow_visibility;
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

    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light.color_intensity.xyz,
        light.color_intensity.w,
        specular_strength,
        shininess);
    return direct * attenuation;
}

vec3 evaluate_packed_light(
    PackedLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    int light_type = light.metadata.x;
    if (light_type == LIGHT_TYPE_DIRECTIONAL)
    {
        return evaluate_directional_light(
            light,
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    if (light_type == LIGHT_TYPE_POINT)
    {
        if (!u_include_local_lights)
            return vec3(0.0);
        return evaluate_point_light(
            light,
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    if (light_type == LIGHT_TYPE_SPOT)
    {
        if (!u_include_local_lights)
            return vec3(0.0);
        return evaluate_spot_light(
            light,
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    if (light_type == LIGHT_TYPE_AREA)
    {
        if (!u_include_local_lights)
            return vec3(0.0);
        return evaluate_area_light(
            light,
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }

    return vec3(0.0);
}

void accumulate_tile_lighting(
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess,
    inout vec3 lighting)
{
    if (u_packed_light_count <= 0)
        return;

    if (!u_compute_culling_enabled || u_tile_count_x <= 0 || u_tile_count_y <= 0 || u_tile_size <= 0)
    {
        for (int index = 0; index < u_packed_light_count; ++index)
        {
            lighting += evaluate_packed_light(
                packed_lights[index],
                world_position,
                normal,
                view_direction,
                albedo,
                specular_strength,
                shininess);
        }
        return;
    }

    ivec2 tile_coord = ivec2(gl_FragCoord.xy) / u_tile_size;
    tile_coord = clamp(tile_coord, ivec2(0), ivec2(u_tile_count_x - 1, u_tile_count_y - 1));
    int tile_index = tile_coord.y * u_tile_count_x + tile_coord.x;
    uvec4 header = tile_headers[tile_index];

    uint tile_offset = header.x;
    uint tile_count = header.y;
    for (uint index = 0U; index < tile_count; ++index)
    {
        uint packed_index = tile_light_indices[tile_offset + index];
        if (packed_index >= uint(u_packed_light_count))
            continue;

        lighting += evaluate_packed_light(
            packed_lights[packed_index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
}

void main()
{
    vec4 albedo_spec = texture(u_gbuffer_albedo_spec, v_tex_coord);
    vec4 normal_data = texture(u_gbuffer_normal, v_tex_coord);
    vec4 material_data = texture(u_gbuffer_material, v_tex_coord);
    float depth = texture(u_scene_depth, v_tex_coord).r;

    if (depth >= 0.999999)
    {
        o_color = vec4(albedo_spec.rgb, 1.0);
        return;
    }

    vec3 world_position = reconstruct_world_position(v_tex_coord, depth);
    vec3 normal = safe_normalize(normal_data.xyz * 2.0 - 1.0, vec3(0.0, 1.0, 0.0));
    vec3 view_direction = safe_normalize(u_camera_position - world_position, vec3(0.0, 0.0, 1.0));
    vec3 albedo = albedo_spec.rgb;
    bool is_unlit = normal_data.a > 0.5;
    float specular_strength = clamp(albedo_spec.a, 0.0, 1.0);
    float shininess = max(material_data.a * 128.0, 1.0);
    vec3 emissive = material_data.rgb;

    if (is_unlit)
    {
        vec3 unlit_color = tbx_linear_to_srgb(albedo + emissive);
        o_color = vec4(unlit_color, 1.0);
        return;
    }

    vec3 lighting = emissive;
    accumulate_tile_lighting(
        world_position,
        normal,
        view_direction,
        albedo,
        specular_strength,
        shininess,
        lighting);

    vec3 bounded_lighting = clamp(lighting, vec3(0.0), vec3(32.0));
    vec3 mapped = tbx_tonemap_aces(bounded_lighting * max(u_exposure, 0.0));
    mapped = tbx_linear_to_srgb(mapped);
    mapped = dither_srgb_output(mapped);
    o_color = vec4(mapped, 1.0);
}
