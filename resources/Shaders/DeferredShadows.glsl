const int SHADOW_CASCADE_COUNT = 3;
const int SHADOW_POISSON_SAMPLE_COUNT = 12;

const vec2 k_shadow_poisson_disk[SHADOW_POISSON_SAMPLE_COUNT] = vec2[](
    vec2(-0.326212, -0.40581),
    vec2(-0.840144, -0.07358),
    vec2(-0.695914, 0.457137),
    vec2(-0.203345, 0.620716),
    vec2(0.96234, -0.194983),
    vec2(0.473434, -0.480026),
    vec2(0.519456, 0.767022),
    vec2(0.185461, -0.893124),
    vec2(0.507431, 0.064425),
    vec2(0.89642, 0.412458),
    vec2(-0.32194, -0.932615),
    vec2(-0.791559, -0.59771));

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
    ShadowCascade cascades[SHADOW_CASCADE_COUNT];
    vec4 cascade_splits;
    vec4 metadata;
};

float tbx_get_cascade_weight(float view_depth, float split_depth, float blend_distance)
{
    return clamp((split_depth - view_depth) / max(blend_distance, 0.0001), 0.0, 1.0);
}

int tbx_get_shadow_cascade_index(float view_depth, ShadowInfo shadow_info)
{
    float cascade_index = 0.0;
    cascade_index += step(shadow_info.cascade_splits.x, view_depth);
    cascade_index += step(shadow_info.cascade_splits.y, view_depth);
    return int(min(cascade_index, float(SHADOW_CASCADE_COUNT - 1)));
}

float tbx_sample_shadow_cascade(
    sampler2DArray shadow_map,
    ShadowInfo shadow_info,
    int cascade_index,
    vec3 normal,
    vec3 light_direction,
    vec3 world_position,
    vec2 pixel_coord)
{
    ShadowCascade cascade = shadow_info.cascades[cascade_index];
    vec4 shadow_position = cascade.light_view_projection * vec4(world_position, 1.0);
    vec3 projected = shadow_position.xyz / max(shadow_position.w, 0.0001);
    projected = (projected * 0.5) + 0.5;
    float inside = step(0.0, projected.x) * step(0.0, projected.y) * step(projected.x, 1.0)
                   * step(projected.y, 1.0) * step(projected.z, 1.0);
    if (inside <= 0.0)
        return 1.0;

    float slope_scale = 1.0 - max(dot(normal, light_direction), 0.0);
    float bias = cascade.depth_bias + (cascade.normal_bias * slope_scale);
    float current_depth = projected.z - bias;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0).xy;
    float kernel_radius = shadow_info.metadata.z * (1.0 + (0.35 * float(cascade_index)));
    float jitter =
        tbx_interleaved_gradient_noise(pixel_coord + vec2(float(cascade_index), current_depth));
    float angle = jitter * 6.28318530718;
    mat2 rotation = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    float visibility = 0.0;

    for (int sample_index = 0; sample_index < SHADOW_POISSON_SAMPLE_COUNT; ++sample_index)
    {
        vec2 offset = rotation * k_shadow_poisson_disk[sample_index];
        offset *= texel_size * kernel_radius;
        float sampled_depth =
            texture(shadow_map, vec3(projected.xy + offset, float(cascade_index))).r;
        visibility += step(current_depth, sampled_depth);
    }

    return visibility / float(SHADOW_POISSON_SAMPLE_COUNT);
}

float tbx_get_shadow_visibility(
    sampler2DArray shadow_map,
    ShadowInfo shadow_info,
    vec3 normal,
    vec3 light_direction,
    vec3 world_position,
    float view_depth,
    vec2 pixel_coord)
{
    int cascade_index = tbx_get_shadow_cascade_index(view_depth, shadow_info);
    float primary_visibility = tbx_sample_shadow_cascade(
        shadow_map,
        shadow_info,
        cascade_index,
        normal,
        light_direction,
        world_position,
        pixel_coord);
    int next_cascade_index = min(cascade_index + 1, SHADOW_CASCADE_COUNT - 1);
    float blend_weight = float(next_cascade_index != cascade_index)
                         * (1.0
                            - tbx_get_cascade_weight(
                                view_depth,
                                shadow_info.cascades[cascade_index].split_depth,
                                shadow_info.cascades[cascade_index].blend_distance));
    float next_visibility = tbx_sample_shadow_cascade(
        shadow_map,
        shadow_info,
        next_cascade_index,
        normal,
        light_direction,
        world_position,
        pixel_coord + vec2(7.0, 13.0));
    return mix(primary_visibility, next_visibility, blend_weight);
}
