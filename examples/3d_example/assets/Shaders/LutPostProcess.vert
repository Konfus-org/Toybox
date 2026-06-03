#version 460

#include "Toybox/Base/ShaderBase.glsl"

layout(location = 0) out vec2 v_tex_coord;

void main()
{
    vec2 position = vec2(
        float((gl_VertexID << 1) & 2),
        float(gl_VertexID & 2));

    v_tex_coord = position;
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
