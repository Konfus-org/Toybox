#version 450 core
#include Globals.glsl
#include TransformUtils.glsl

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_texcoord;
layout(location = 4) in vec4 a_tangent;
layout(location = 5) in vec4 a_model0;
layout(location = 6) in vec4 a_model1;
layout(location = 7) in vec4 a_model2;
layout(location = 8) in vec4 a_model3;

out vec4 v_color;
out vec2 v_tex_coord;
out vec3 v_world_position;
out vec3 v_world_normal;
out vec3 v_world_tangent;
out float v_world_tangent_sign;

void main()
{
    v_color = u_material_uniforms[0];
    v_tex_coord = a_texcoord;

    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);
    vec4 world_position = model * vec4(a_position, 1.0);
    v_world_position = world_position.xyz;
    mat3 normal_matrix = mat3(transpose(inverse(model)));
    v_world_normal = normalize(normal_matrix * a_normal);
    v_world_tangent = normalize(normal_matrix * a_tangent.xyz);
    v_world_tangent_sign = a_tangent.w;

    gl_Position = u_view_proj * world_position;
}
