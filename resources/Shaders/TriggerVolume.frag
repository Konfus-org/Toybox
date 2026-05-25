#version 450

#include "Toybox/ShaderBase.glsl"

layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_world_normal;
layout(location = 3) in vec2 v_tex_coord;

layout(location = 0) out vec4 o_color;

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxTriggerVolumeMaterialData
{
    vec4 u_albedo_color;

    float u_backface_alpha;
    vec3 _tbx_pad_u_backface_alpha;
    float u_rim_strength;
    vec3 _tbx_pad_u_rim_strength;
    float u_rim_power;
    vec3 _tbx_pad_u_rim_power;
};

layout(binding = TBX_BINDING_ALBEDO_MAP) uniform sampler2D u_albedo_map;

void main()
{
    vec4 color = texture(u_albedo_map, v_tex_coord) * u_albedo_color;

    vec3 normal = normalize(v_world_normal);
    vec3 view_direction = normalize(u_camera_world_position.xyz - v_world_position);
    float edge = pow(1.0 - saturate(abs(dot(normal, view_direction))), max(u_rim_power, 0.001));
    float face_alpha = gl_FrontFacing ? 1.0 : saturate(u_backface_alpha);

    color.a = saturate(color.a * face_alpha + color.a * edge * saturate(u_rim_strength));
    o_color = color;
}
