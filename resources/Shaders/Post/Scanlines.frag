#include "Post.glsl"

// Custom post-processing effect: scrolling CRT scanlines. Authored as a normal material:
//   params lanes (declared .mat order, packed float stream):
//     params[0]   = tint (rgba, multiplied over the result)
//     params[1].x = density (number of scanlines down the screen)
//     params[1].y = intensity (0 = invisible, 1 = fully dark troughs)
//     params[1].z = scroll_speed (lines per second the pattern drifts; uses elapsed time)
// Demonstrates reading the elapsed-time scene uniform (cameraPositionTime.w) from a post effect.
layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

void main()
{
    vec3 source = tbx_post_input(v_tex_coord).rgb;
    vec4 tint = tbx_post_param(0u);
    vec4 controls = tbx_post_param(1u);
    float density = max(controls.x, 1.0);
    float intensity = tbx_saturate(controls.y);
    float scroll_speed = controls.z;
    float time = cameraPositionTime.w;

    // A bright/dark band travelling down the screen over time.
    float wave = sin((v_tex_coord.y * density - time * scroll_speed) * TBX_PI * 2.0) * 0.5 + 0.5;
    float mask = 1.0 - intensity * (1.0 - wave);
    vec3 effect = source * mask * tint.rgb;

    o_color = vec4(tbx_post_apply(v_tex_coord, effect), 1.0);
}
