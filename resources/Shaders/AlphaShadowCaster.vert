#version 450

#include "Toybox/Shadows/ShadowCaster.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 3) in vec2 a_tex_coord;

layout(location = 0) out vec2 v_tex_coord;

void main()
{
    v_tex_coord = a_tex_coord;
    tbx_default_shadow_vertex(a_position);
}
