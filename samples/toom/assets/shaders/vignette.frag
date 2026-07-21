#version 460 core
// Vignette + animated film grain.
in vec2 v_uv;

uniform sampler2D u_scene;
uniform vec2 u_resolution;
uniform float u_time;

out vec4 out_color;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    const vec3 color = texture(u_scene, v_uv).rgb;
    const vec2 centered = v_uv - 0.5;
    const float vignette = 1.0 - dot(centered, centered) * 0.9;
    const float grain = (hash(v_uv * u_resolution + fract(u_time) * 61.0) - 0.5) * 0.06;
    out_color = vec4(color * vignette + grain, 1.0);
}
