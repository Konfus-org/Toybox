#version 450 core

in vec2 v_uv;
out vec4 o_color;

uniform sampler2D u_scene_color;
uniform float u_blend;

void main()
{
    const vec3 source = texture(u_scene_color, v_uv).rgb;
    const float luminance = dot(source, vec3(0.2126, 0.7152, 0.0722));
    const vec3 graded = mix(source, vec3(luminance), clamp(u_blend, 0.0, 1.0));
    o_color = vec4(graded, 1.0);
}
