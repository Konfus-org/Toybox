#version 450

#include "Toybox/Lighting/ShadowSampling.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

layout(binding = TBX_BINDING_GBUFFER_NORMAL)
uniform sampler2D u_gbuffer_normal;

layout(binding = TBX_BINDING_GBUFFER_DEPTH)
uniform sampler2D u_gbuffer_depth;

void main()
{
    vec3 normal = tbx_unpack_normal(texture(u_gbuffer_normal, v_tex_coord).xyz);
    float depth = texture(u_gbuffer_depth, v_tex_coord).r;
    vec3 world_position = tbx_reconstruct_world_position(v_tex_coord, depth);
    vec3 light_direction = normalize(u_light_direction.xyz);

    float visibility = tbx_sample_shadow(world_position, normal, light_direction);
    o_color = vec4(vec3(visibility), 1.0);
}
