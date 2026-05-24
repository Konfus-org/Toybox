#version 450

#include "Toybox/ShaderBase.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;
layout(location = 3) in vec2 a_tex_coord;
layout(location = 4) in vec4 a_color;
layout(location = 5) in mat4 a_instance_model;
layout(location = 9) in mat4 a_instance_normal;

layout(location = 0) out vec3 v_world_position;
layout(location = 1) out vec3 v_world_normal;
layout(location = 2) out vec4 v_world_tangent;
layout(location = 3) out vec2 v_tex_coord;

void main()
{
    vec4 world_position = a_instance_model * vec4(a_position, 1.0);

    v_world_position = world_position.xyz;
    v_world_normal = normalize((a_instance_normal * vec4(a_normal, 0.0)).xyz);
    v_world_tangent = vec4(normalize((a_instance_model * vec4(a_tangent.xyz, 0.0)).xyz), a_tangent.w);
    v_tex_coord = a_tex_coord;

    gl_Position = u_view_projection * world_position;
}
