#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 8) in mat4 a_model;

uniform mat4 u_light_view_projection = mat4(1.0);

void main()
{
    gl_Position = u_light_view_projection * (tbx_get_model_matrix(a_model) * vec4(a_position, 1.0));
}
