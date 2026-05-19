#include "Toybox/ShaderBase.glsl"

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

void tbx_default_shadow_vertex(vec3 position)
{
    vec4 world_position = u_model * vec4(position, 1.0);
    gl_Position = u_light_view_projection * world_position;
}
