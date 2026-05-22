#version 450

#include "Toybox/Materials/UnlitMaterial.glsl"

layout(location = 0) in vec2 v_tex_coord;

layout(location = 0) out vec4 o_color;

void main()
{
    o_color = tbx_build_unlit_color(v_tex_coord);
}
