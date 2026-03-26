#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 3) in vec2 a_texcoord;

out vec2 v_tex_coord;

layout(std140, binding = 1) uniform MaterialParams
{
    vec4 u_color;
    vec4 u_emissive;
};

void main()
{
    v_tex_coord = a_texcoord;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
