#version 450

#include "Toybox/Shadows/ShadowCaster.glsl"

layout(location = 0) in vec3 a_position;

void main()
{
    tbx_default_shadow_vertex(a_position);
}
