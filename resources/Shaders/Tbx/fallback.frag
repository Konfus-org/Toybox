#version 460 core
// The loud fallback for broken renderables: the failure mode's color, drawn as a flashing
// emissive over the bound albedo (plain white, or the debug checkerboard for missing textures).
// Emissive + unlit so it shows at full strength regardless of scene lighting, and the pulse
// makes broken things blink for attention. u_time is wall-clock seconds from the renderer.
in vec2 v_uv;

uniform sampler2D u_albedo;
uniform vec4 u_tint;
uniform float u_time;

out vec4 out_color;

void main()
{
    vec3 emissive = texture(u_albedo, v_uv).rgb * u_tint.rgb;
    // Pulse the emissive brightness ~1.4 Hz: dim, then push past 1.0 so any bloom catches it.
    float pulse = 0.5 + 0.5 * sin(u_time * 9.0);
    float flash = mix(0.25, 1.75, pulse);
    out_color = vec4(emissive * flash, 1.0);
}
