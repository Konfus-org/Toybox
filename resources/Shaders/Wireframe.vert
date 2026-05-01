#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 5) in vec4 a_model0;
layout(location = 6) in vec4 a_model1;
layout(location = 7) in vec4 a_model2;
layout(location = 8) in vec4 a_model3;

out vec4 v_color;
out vec3 v_world_normal;

void main()
{
    v_color = u_color;

    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);
    vec4 world_position = tbx_get_model_matrix(model) * vec4(a_position, 1.0);
    mat3 normal_matrix = mat3(transpose(inverse(tbx_get_model_matrix(model))));
    v_world_normal = normalize(normal_matrix * a_normal);

    gl_Position = u_view_proj * world_position;
}
