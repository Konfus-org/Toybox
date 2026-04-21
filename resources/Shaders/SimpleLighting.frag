#version 450 core

in vec2 v_uv;
out vec4 o_color;

uniform sampler2D u_scene_color;
uniform vec3 u_light_gain;
uniform vec3 u_light_add;

void main()
{
    const vec3 scene = texture(u_scene_color, v_uv).rgb;
    o_color = vec4(scene * u_light_gain + u_light_add, 1.0);
}
