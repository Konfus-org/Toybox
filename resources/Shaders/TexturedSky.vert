#version 450

#include "Toybox/ShaderBase.glsl"

layout(location = 0) in vec3 a_position;

layout(location = 0) out vec3 v_sky_direction;

void main()
{
    vec3 geometry_direction = normalize(a_position);
    vec3 sky_direction = normalize(mat3(u_model) * geometry_direction);
    mat4 view_without_translation = mat4(mat3(u_view));
    vec4 clip_position = u_projection * view_without_translation * vec4(geometry_direction, 1.0);

    v_sky_direction = sky_direction;
    gl_Position = clip_position.xyww;
}
