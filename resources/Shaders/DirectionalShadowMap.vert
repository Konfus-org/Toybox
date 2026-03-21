#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 8) in mat4 a_model;

uniform mat4 u_light_view_projection = mat4(1.0);

mat4 tbx_get_model_matrix(const mat4 model)
{
    return model;
}

void main()
{
    gl_Position = u_light_view_projection * (tbx_get_model_matrix(a_model) * vec4(a_position, 1.0));
}
