#version 460 core
// Toybox builtin sky fragment stage: equirectangular sample along the view ray.
in vec2 v_ndc;

uniform mat4 u_inverse_view_projection;
uniform vec3 u_camera_position;
uniform vec4 u_tint;
uniform sampler2D u_sky;

out vec4 out_color;

const float PI = 3.14159265;

void main()
{
    const vec4 far_point = u_inverse_view_projection * vec4(v_ndc, 1.0, 1.0);
    const vec3 direction = normalize(far_point.xyz / far_point.w - u_camera_position);
    const float u = 0.5 + atan(direction.x, -direction.z) / (2.0 * PI);
    const float v = 0.5 - asin(clamp(direction.y, -1.0, 1.0)) / PI;
    out_color = vec4(texture(u_sky, vec2(u, v)).rgb * u_tint.rgb, 1.0);
}
