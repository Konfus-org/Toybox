#version 460 core
// Chunky pixels + posterized color — the crunchy doom look.
in vec2 v_uv;

uniform sampler2D u_scene;
uniform vec2 u_resolution;
uniform float u_time;

out vec4 out_color;

void main()
{
    const vec2 pixel_size = 3.0 / u_resolution;
    const vec2 uv = (floor(v_uv / pixel_size) + 0.5) * pixel_size;
    vec3 color = texture(u_scene, uv).rgb;
    color = floor(color * 8.0) / 8.0;
    out_color = vec4(color, 1.0);
}
