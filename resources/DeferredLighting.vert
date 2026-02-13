#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 3) in vec2 a_texcoord;

out vec2 v_tex_coord;

void main()
{
    v_tex_coord = a_texcoord;
    gl_Position = vec4(a_position, 1.0);
}
