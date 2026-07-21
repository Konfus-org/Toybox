#version 460 core
// Toybox builtin lit vertex stage. Layout everywhere: position(3) + normal(3) + uv(2).
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform mat4 u_light_view_projection;

out VertexData
{
    vec3 world_position;
    vec3 world_normal;
    vec4 shadow_coords;
    vec2 uv;
} vertex;

void main()
{
    const vec4 world = u_model * vec4(in_position, 1.0);
    vertex.world_position = world.xyz;
    vertex.world_normal = mat3(u_model) * in_normal;
    vertex.shadow_coords = u_light_view_projection * world;
    vertex.uv = in_uv;
    gl_Position = u_view_projection * world;
}
