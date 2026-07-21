#version 460 core
// The health bar's composite stage: a sine uv-warp over its UI layer texture.
in vec2 v_uv;

uniform sampler2D u_ui;
uniform float u_time;

out vec4 out_color;

void main()
{
    vec2 uv = v_uv;
    uv.y += sin(u_time * 5.0 + uv.x * 60.0) * 0.004;
    out_color = texture(u_ui, uv);
}
