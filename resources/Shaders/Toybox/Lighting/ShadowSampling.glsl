#include "Toybox/ShaderBase.glsl"

layout(binding = TBX_BINDING_SHADOW_MAP) uniform sampler2DArrayShadow u_shadow_map;

layout(std140, binding = TBX_BINDING_SHADOW_PASS_DATA) uniform TbxShadowData
{
    mat4 u_light_view_projections[TBX_MAX_LIGHTS];
    vec4 u_light_directions[TBX_MAX_LIGHTS];
    vec4 u_shadow_params[TBX_MAX_LIGHTS];
    vec4 u_shadow_extra_params[TBX_MAX_LIGHTS];
    ivec4 u_shadow_meta;
};

vec3 tbx_project_shadow_coord(vec3 world_position, int shadow_index)
{
    vec4 light_position = u_light_view_projections[shadow_index] * vec4(world_position, 1.0);
    vec3 projected = light_position.xyz / max(light_position.w, TBX_EPSILON);
    return projected * 0.5 + 0.5;
}

float tbx_sample_shadow_hardware(vec3 shadow_coord, float bias, int shadow_index)
{
    if (shadow_coord.z < 0.0 || shadow_coord.z > 1.0)
    {
        return 1.0;
    }

    if (shadow_coord.x < 0.0 || shadow_coord.x > 1.0 ||
        shadow_coord.y < 0.0 || shadow_coord.y > 1.0)
    {
        return 1.0;
    }

    return texture(u_shadow_map, vec4(shadow_coord.xy, shadow_index, shadow_coord.z - bias));
}

float tbx_sample_shadow_pcf(vec3 shadow_coord, float bias, int shadow_index)
{
    vec2 texel_size = 1.0 / vec2(textureSize(u_shadow_map, 0).xy);
    float visibility = tbx_sample_shadow_hardware(shadow_coord, bias, shadow_index);
    visibility += tbx_sample_shadow_hardware(
        vec3(shadow_coord.xy + vec2(-0.94201624, -0.39906216) * texel_size, shadow_coord.z),
        bias,
        shadow_index);
    visibility += tbx_sample_shadow_hardware(
        vec3(shadow_coord.xy + vec2(0.94558609, -0.76890725) * texel_size, shadow_coord.z),
        bias,
        shadow_index);
    visibility += tbx_sample_shadow_hardware(
        vec3(shadow_coord.xy + vec2(-0.09418410, -0.92938870) * texel_size, shadow_coord.z),
        bias,
        shadow_index);
    visibility += tbx_sample_shadow_hardware(
        vec3(shadow_coord.xy + vec2(0.34495938, 0.29387760) * texel_size, shadow_coord.z),
        bias,
        shadow_index);

    return visibility * 0.2;
}

float tbx_sample_shadow_layer(
    vec3 world_position,
    vec3 normal,
    vec3 light_direction,
    int shadow_index)
{
    if (shadow_index < 0 || shadow_index >= u_shadow_meta.x)
    {
        return 1.0;
    }

    vec4 shadow_params = u_shadow_params[shadow_index];
    float shadow_depth_bias = shadow_params.x;
    float shadow_normal_bias = shadow_params.y;
    float shadow_strength = shadow_params.z;
    float shadow_slope_bias = shadow_params.w;
    float ndl = max(dot(normal, -light_direction), 0.0);
    float slope_bias = shadow_slope_bias * (1.0 - ndl);
    float depth_bias = shadow_depth_bias + slope_bias;
    vec3 biased_position = world_position + normal * shadow_normal_bias;
    vec3 shadow_coord = tbx_project_shadow_coord(biased_position, shadow_index);

    float visibility = tbx_sample_shadow_pcf(shadow_coord, depth_bias, shadow_index);
    return mix(1.0 - saturate(shadow_strength), 1.0, visibility);
}

int tbx_select_shadow_cascade(float view_depth, int shadow_index, int shadow_layer_count)
{
    int selected_index = shadow_index;
    for (int cascade_offset = 0; cascade_offset < shadow_layer_count; ++cascade_offset)
    {
        int cascade_index = shadow_index + cascade_offset;
        selected_index = cascade_index;
        if (view_depth <= u_shadow_extra_params[cascade_index].y)
        {
            break;
        }
    }

    return selected_index;
}

float tbx_sample_shadow(
    vec3 world_position,
    vec3 normal,
    vec3 light_direction,
    int shadow_index,
    int shadow_layer_count)
{
    if (shadow_index < 0 || shadow_index >= u_shadow_meta.x)
    {
        return 1.0;
    }

    int valid_layer_count = clamp(shadow_layer_count, 1, u_shadow_meta.x - shadow_index);
    if (valid_layer_count <= 1)
    {
        return tbx_sample_shadow_layer(world_position, normal, light_direction, shadow_index);
    }

    float view_depth = -(u_view * vec4(world_position, 1.0)).z;
    int selected_index = tbx_select_shadow_cascade(view_depth, shadow_index, valid_layer_count);
    float visibility = tbx_sample_shadow_layer(
        world_position,
        normal,
        light_direction,
        selected_index);

    int selected_offset = selected_index - shadow_index;
    if (selected_offset + 1 < valid_layer_count)
    {
        float blend_start = u_shadow_extra_params[selected_index].z;
        float blend_end = u_shadow_extra_params[selected_index].y;
        float blend = smoothstep(blend_start, blend_end, view_depth);
        if (blend > 0.0)
        {
            float next_visibility = tbx_sample_shadow_layer(
                world_position,
                normal,
                light_direction,
                selected_index + 1);
            visibility = mix(visibility, next_visibility, blend);
        }
    }

    return visibility;
}
