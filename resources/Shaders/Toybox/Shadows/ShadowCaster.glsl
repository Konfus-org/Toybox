#include "Toybox/ShaderBase.glsl"

layout(std140, binding = TBX_BINDING_SHADOW_PASS_DATA) uniform TbxShadowPassData
{
    mat4 u_light_view_projection;
    vec4 u_light_direction;

    // x = depth bias
    // y = normal bias
    // z = shadow strength
    // w = unused
    vec4 u_shadow_params0;
};

void tbx_default_shadow_vertex(vec3 position)
{
    vec4 world_position = u_model * vec4(position, 1.0);
    gl_Position = u_light_view_projection * world_position;
}
