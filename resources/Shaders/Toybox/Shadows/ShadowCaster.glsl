#include "Toybox/ShaderBase.glsl"

layout(std140, binding = TBX_BINDING_SHADOW_PASS_DATA) uniform TbxShadowData
{
    mat4 u_light_view_projections[TBX_MAX_LIGHTS];
    vec4 u_light_directions[TBX_MAX_LIGHTS];
    vec4 u_shadow_params[TBX_MAX_LIGHTS];
    vec4 u_shadow_extra_params[TBX_MAX_LIGHTS];
    ivec4 u_shadow_meta;
};

void tbx_default_shadow_vertex(vec3 position)
{
    vec4 world_position = u_model * vec4(position, 1.0);
    gl_Position = u_light_view_projections[u_shadow_meta.y] * world_position;
}

void tbx_default_shadow_vertex(vec3 position, mat4 model)
{
    vec4 world_position = model * vec4(position, 1.0);
    gl_Position = u_light_view_projections[u_shadow_meta.y] * world_position;
}
