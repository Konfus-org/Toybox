#version 450

#include "Toybox/Shadows/ShadowCaster.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 5) in mat4 a_instance_model;

void main()
{
    tbx_default_shadow_vertex(a_position, a_instance_model);
}
