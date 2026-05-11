#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 5) in vec4 a_model0;
layout(location = 6) in vec4 a_model1;
layout(location = 7) in vec4 a_model2;
layout(location = 8) in vec4 a_model3;

layout(std140, binding = 0) uniform ToyboxViewBlock
{
    mat4 u_view_proj;
};

void main()
{
    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);
    gl_Position = u_view_proj * model * vec4(a_position, 1.0);
}
