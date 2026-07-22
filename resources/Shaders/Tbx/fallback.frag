#version 460 core
// The loud unlit fallback: the failure mode's color over the bound albedo (plain white, or
// the debug checkerboard for missing textures).
in vec2 v_uv;

uniform sampler2D u_albedo;
uniform vec4 u_tint;

out vec4 out_color;

void main()
{
    out_color = vec4(texture(u_albedo, v_uv).rgb * u_tint.rgb, 1.0);
}
