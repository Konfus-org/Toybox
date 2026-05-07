#version 450 core
#include Globals.glsl

layout(location = 0) in vec3 a_position;
layout(location = 5) in vec4 a_model0;
layout(location = 6) in vec4 a_model1;
layout(location = 7) in vec4 a_model2;
layout(location = 8) in vec4 a_model3;

out vec3 v_local_direction;

void main()
{
    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);
    v_local_direction = mat3(model) * normalize(a_position);
    gl_Position = u_view_proj * model * vec4(a_position, 1.0);
}
