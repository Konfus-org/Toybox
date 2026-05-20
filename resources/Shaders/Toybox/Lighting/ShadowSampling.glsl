#include "Toybox/ShaderBase.glsl"

layout(binding = TBX_BINDING_SHADOW_MAP) uniform sampler2D u_shadow_map;

layout(std140, binding = TBX_BINDING_SHADOW_PASS_DATA) uniform TbxShadowPassData
{
    mat4 u_light_view_projection;
    vec4 u_light_direction;
    float u_shadow_depth_bias;
    vec3 _tbx_pad_u_shadow_depth_bias;
    float u_shadow_normal_bias;
    vec3 _tbx_pad_u_shadow_normal_bias;
    float u_shadow_strength;
    vec3 _tbx_pad_u_shadow_strength;
};

vec3 tbx_project_shadow_coord(vec3 world_position)
{
    vec4 light_position = u_light_view_projection * vec4(world_position, 1.0);
    vec3 projected = light_position.xyz / max(light_position.w, TBX_EPSILON);
    return projected * 0.5 + 0.5;
}

float tbx_sample_shadow_pcf(vec3 shadow_coord, float bias)
{
    if (shadow_coord.z > 1.0)
    {
        return 1.0;
    }

    if (shadow_coord.x < 0.0 || shadow_coord.x > 1.0 ||
        shadow_coord.y < 0.0 || shadow_coord.y > 1.0)
    {
        return 1.0;
    }

    vec2 texel_size = 1.0 / vec2(textureSize(u_shadow_map, 0));
    float visibility = 0.0;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 offset = vec2(x, y) * texel_size;
            float shadow_depth = texture(u_shadow_map, shadow_coord.xy + offset).r;
            visibility += shadow_coord.z - bias <= shadow_depth ? 1.0 : 0.0;
        }
    }

    return visibility / 9.0;
}

float tbx_sample_shadow(vec3 world_position, vec3 normal, vec3 light_direction)
{
    float ndl = max(dot(normal, -light_direction), 0.0);
    vec3 biased_position = world_position + normal * u_shadow_normal_bias * (1.0 - ndl);
    vec3 shadow_coord = tbx_project_shadow_coord(biased_position);

    float visibility = tbx_sample_shadow_pcf(shadow_coord, u_shadow_depth_bias);
    return mix(1.0 - saturate(u_shadow_strength), 1.0, visibility);
}
