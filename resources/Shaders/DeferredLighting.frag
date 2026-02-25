#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_color;

in vec2 v_tex_coord;

uniform sampler2D u_gbuffer_albedo_spec;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_scene_depth;

// NOTE: For this to work, shadow maps must be depth textures with:
// GL_TEXTURE_COMPARE_MODE = GL_COMPARE_REF_TO_TEXTURE
// GL_TEXTURE_COMPARE_FUNC = GL_LEQUAL (or GL_LESS if that matches your setup)
uniform sampler2DShadow u_shadow_maps[24];

uniform vec3 u_camera_position = vec3(0.0);
uniform vec3 u_camera_forward = vec3(0.0, 0.0, -1.0);
uniform mat4 u_inverse_view_projection = mat4(1.0);
uniform mat4 u_light_view_projection_matrices[24];
uniform int u_directional_light_count = 0;
uniform int u_point_light_count = 0;
uniform int u_spot_light_count = 0;
uniform int u_area_light_count = 0;
uniform int u_shadow_map_count = 0;
uniform float u_cascade_splits[4] = float[4](10.0, 25.0, 60.0, 1000.0);
uniform float u_shadow_softness = 1.75;
uniform float u_exposure = 0.6;

struct DirectionalLight
{
    vec3 direction;
    float intensity;
    vec3 color;
    float ambient;
    int shadow_map_index;
};

struct PointLight
{
    vec3 position;
    float range;
    vec3 color;
    float intensity;
    int shadow_map_index;
};

struct SpotLight
{
    vec3 position;
    float range;
    vec3 direction;
    float inner_cos;
    vec3 color;
    float outer_cos;
    float intensity;
    int shadow_map_index;
};

struct AreaLight
{
    vec3 position;
    float range;
    vec3 direction;
    float intensity;
    vec3 color;
    vec2 area_size;
};

const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_POINT_LIGHTS = 32;
const int MAX_SPOT_LIGHTS = 16;
const int MAX_AREA_LIGHTS = 16;
const int DIRECTIONAL_SHADOW_CASCADE_COUNT = 2;
const int POINT_SHADOW_FACE_COUNT = 6;

// Kernel radii are now "intent" only; the sampling path is edge-adaptive and uses a tiny kernel.
// Keep these so your call sites stay intact.
const int DIRECTIONAL_PCF_KERNEL_RADIUS = 2;
const int LOCAL_PCF_KERNEL_RADIUS = 1;

const float DIRECTIONAL_CASCADE_TRANSITION_RATIO = 0.1;
const float OUTPUT_DITHER_STRENGTH = 1.0 / 255.0;

uniform DirectionalLight u_directional_lights[MAX_DIRECTIONAL_LIGHTS];
uniform PointLight u_point_lights[MAX_POINT_LIGHTS];
uniform SpotLight u_spot_lights[MAX_SPOT_LIGHTS];
uniform AreaLight u_area_lights[MAX_AREA_LIGHTS];

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

// -----------------------------
// Shadow sampling
// ---------------

// Shadow-only PCF sampling:
// - stable weighted kernel (no stochastic noise, no screen-space striping)
// - softened edges while keeping fully lit/shadowed regions flat
float sample_shadow_visibility(
    int shadow_map_index,
    vec2 shadow_uv,
    float current_depth,
    float bias,
    float softness_scale,
    int kernel_radius)
{
    // Guard against bad indices (some drivers still try to sample...)
    if (shadow_map_index < 0 || shadow_map_index >= u_shadow_map_count)
        return 1.0;

    // Radius in texels. Scale by kernel intent so directional shadows can be softer
    // than local lights without affecting other lighting paths.
    float radius_texels =
        clamp(u_shadow_softness * softness_scale * max(float(kernel_radius), 1.0), 0.0, 3.0);

    // Compare reference (depth) for sampler2DShadow is the 3rd component.
    float ref = clamp(current_depth - bias, 0.0, 1.0);

    // 1-tap center compare
    float center = texture(u_shadow_maps[shadow_map_index], vec3(shadow_uv, ref));

    // If softness is effectively off, return the center result.
    if (radius_texels <= 0.001)
        return center;

    vec2 texel = 1.0 / vec2(textureSize(u_shadow_maps[shadow_map_index], 0));
    vec2 r = texel * radius_texels;

    // 3x3 tent PCF kernel.
    float sum = 0.0;
    float weight_sum = 0.0;
    const float center_weight = 4.0;
    const float axis_weight = 2.0;
    const float corner_weight = 1.0;

    sum += center * center_weight;
    weight_sum += center_weight;

    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(-r.x, 0.0), ref))
        * axis_weight;
    weight_sum += axis_weight;
    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(r.x, 0.0), ref))
        * axis_weight;
    weight_sum += axis_weight;
    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(0.0, -r.y), ref))
        * axis_weight;
    weight_sum += axis_weight;
    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(0.0, r.y), ref))
        * axis_weight;
    weight_sum += axis_weight;

    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(-r.x, -r.y), ref))
        * corner_weight;
    weight_sum += corner_weight;
    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(r.x, -r.y), ref))
        * corner_weight;
    weight_sum += corner_weight;
    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(-r.x, r.y), ref))
        * corner_weight;
    weight_sum += corner_weight;
    sum += texture(
        u_shadow_maps[shadow_map_index],
        vec3(shadow_uv + vec2(r.x, r.y), ref))
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
        return false;

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

float sample_projected_shadow_visibility(
    int shadow_map_index,
    vec4 light_space_position,
    float bias,
    float softness_scale,
    int kernel_radius)
{
    float visibility = 1.0;
    try_sample_projected_shadow_visibility(
        shadow_map_index,
        light_space_position,
        bias,
        softness_scale,
        kernel_radius,
        visibility);
    return visibility;
}

vec3 apply_shadow_normal_offset(
    vec3 world_position,
    vec3 normal,
    float ndotl,
    float base_offset,
    float slope_scale)
{
    float slope_term = 1.0 - clamp(ndotl, 0.0, 1.0);
    float offset = max(base_offset + (slope_term * slope_scale), 0.0);
    return world_position + (normal * offset);
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

    vec3 shadow_position = apply_shadow_normal_offset(
        world_position,
        normal,
        ndotl,
        normal_offset_base,
        normal_offset_slope);

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

vec3 evaluate_directional_light(
    DirectionalLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 light_direction = safe_normalize(light.direction, vec3(0.0, 0.0, -1.0));
    vec3 ambient = albedo * light.color * max(light.ambient, 0.0);
    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light.color,
        light.intensity,
        specular_strength,
        shininess);

    float shadow_visibility = 1.0;
    int base_shadow_map_index = light.shadow_map_index;
    if (base_shadow_map_index >= 0)
    {
        const int cascade0_shadow_map_index = base_shadow_map_index;
        const int cascade1_shadow_map_index = base_shadow_map_index + 1;
        const bool has_second_cascade =
            cascade1_shadow_map_index < u_shadow_map_count && DIRECTIONAL_SHADOW_CASCADE_COUNT > 1;

        // Keep near cascade tighter and far cascade softer.
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
            const float blend_width =
                max(split_depth * DIRECTIONAL_CASCADE_TRANSITION_RATIO, 0.0001);
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
                const float blend_softness =
                    mix(cascade0_softness, cascade1_softness, cascade_blend_t);
                const float blend_bias_scale = mix(0.0008, 0.0014, cascade_blend_t);
                const float blend_min_bias = mix(0.00015, 0.00035, cascade_blend_t);
                const float blend_normal_offset_base = mix(0.0025, 0.0040, cascade_blend_t);
                const float blend_normal_offset_slope = mix(0.012, 0.018, cascade_blend_t);

                float cascade0_visibility = 1.0;
                float cascade1_visibility = 1.0;

                const bool has_cascade0_visibility = try_sample_light_shadow_visibility(
                    cascade0_shadow_map_index,
                    world_position,
                    normal,
                    light_direction,
                    blend_bias_scale,
                    blend_min_bias,
                    blend_normal_offset_base,
                    blend_normal_offset_slope,
                    blend_softness,
                    DIRECTIONAL_PCF_KERNEL_RADIUS,
                    cascade0_visibility);

                const bool has_cascade1_visibility = try_sample_light_shadow_visibility(
                    cascade1_shadow_map_index,
                    world_position,
                    normal,
                    light_direction,
                    blend_bias_scale,
                    blend_min_bias,
                    blend_normal_offset_base,
                    blend_normal_offset_slope,
                    blend_softness,
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
    PointLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 to_light = light.position - world_position;
    float distance_to_light = length(to_light);
    if (distance_to_light >= light.range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    float attenuation = calc_distance_attenuation(distance_to_light, light.range);
    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light.color,
        light.intensity,
        specular_strength,
        shininess);

    float shadow_visibility = 1.0;
    int base_shadow_map_index = light.shadow_map_index;
    if (base_shadow_map_index >= 0
        && base_shadow_map_index + (POINT_SHADOW_FACE_COUNT - 1) < u_shadow_map_count)
    {
        vec3 light_to_fragment = world_position - light.position;
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
    SpotLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 to_light = light.position - world_position;
    float distance_to_light = length(to_light);
    if (distance_to_light >= light.range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    vec3 spot_axis = safe_normalize(-light.direction, vec3(0.0, 0.0, -1.0));
    float spot_cos = dot(light_direction, spot_axis);
    if (spot_cos <= light.outer_cos)
        return vec3(0.0);

    float cone = clamp(
        (spot_cos - light.outer_cos) / max(light.inner_cos - light.outer_cos, 0.0001),
        0.0,
        1.0);
    float attenuation = calc_distance_attenuation(distance_to_light, light.range) * cone;

    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light.color,
        light.intensity,
        specular_strength,
        shininess);

    float shadow_visibility = 1.0;
    int shadow_map_index = light.shadow_map_index;
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
    AreaLight light,
    vec3 world_position,
    vec3 normal,
    vec3 view_direction,
    vec3 albedo,
    float specular_strength,
    float shininess)
{
    vec3 to_light = light.position - world_position;
    float distance_to_light = length(to_light);
    if (distance_to_light >= light.range)
        return vec3(0.0);

    vec3 light_direction = to_light / max(distance_to_light, 0.0001);
    vec3 emission_axis = safe_normalize(light.direction, vec3(0.0, -1.0, 0.0));
    vec3 light_to_fragment_direction = -light_direction;
    float front_cos = dot(light_to_fragment_direction, emission_axis);
    if (front_cos <= 0.0)
        return vec3(0.0);

    vec3 tangent = vec3(1.0, 0.0, 0.0);
    vec3 bitangent = vec3(0.0, 0.0, 1.0);
    build_light_basis(emission_axis, tangent, bitangent);

    vec2 half_size = max(light.area_size * 0.5, vec2(0.001));
    vec3 local_offset = world_position - light.position;
    float normalized_tangent_distance = abs(dot(local_offset, tangent)) / half_size.x;
    float normalized_bitangent_distance = abs(dot(local_offset, bitangent)) / half_size.y;
    float tangent_falloff =
        1.0 / (1.0 + (normalized_tangent_distance * normalized_tangent_distance));
    float bitangent_falloff =
        1.0 / (1.0 + (normalized_bitangent_distance * normalized_bitangent_distance));
    float footprint = tangent_falloff * bitangent_falloff;
    if (footprint <= 0.0)
        return vec3(0.0);

    float attenuation = calc_distance_attenuation(distance_to_light, light.range);
    attenuation *= front_cos * footprint;

    vec3 direct = evaluate_lighting_brdf(
        normal,
        view_direction,
        light_direction,
        albedo,
        light.color,
        light.intensity,
        specular_strength,
        shininess);
    return direct * attenuation;
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
    int directional_light_count = min(u_directional_light_count, MAX_DIRECTIONAL_LIGHTS);
    int point_light_count = min(u_point_light_count, MAX_POINT_LIGHTS);
    int spot_light_count = min(u_spot_light_count, MAX_SPOT_LIGHTS);
    int area_light_count = min(u_area_light_count, MAX_AREA_LIGHTS);

    for (int index = 0; index < directional_light_count; ++index)
    {
        lighting += evaluate_directional_light(
            u_directional_lights[index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    for (int index = 0; index < point_light_count; ++index)
    {
        lighting += evaluate_point_light(
            u_point_lights[index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    for (int index = 0; index < spot_light_count; ++index)
    {
        lighting += evaluate_spot_light(
            u_spot_lights[index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }
    for (int index = 0; index < area_light_count; ++index)
    {
        lighting += evaluate_area_light(
            u_area_lights[index],
            world_position,
            normal,
            view_direction,
            albedo,
            specular_strength,
            shininess);
    }

    vec3 bounded_lighting = clamp(lighting, vec3(0.0), vec3(32.0));
    vec3 mapped = tbx_tonemap_aces(bounded_lighting * max(u_exposure, 0.0));
    mapped = tbx_linear_to_srgb(mapped);
    mapped = dither_srgb_output(mapped);
    o_color = vec4(mapped, 1.0);
}
