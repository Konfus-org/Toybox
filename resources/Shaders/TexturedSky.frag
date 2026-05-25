#version 450

#include "Toybox/Materials/TexturedSkyMaterial.glsl"

layout(location = 0) in vec3 v_sky_direction;

layout(location = 0) out vec4 o_color;

void main()
{
    o_color = tbx_sample_textured_sky_color(v_sky_direction);
}
