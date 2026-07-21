#include "ShaderBase.glsl"

// Depth-only: the fixed-function depth write is the entire output. The one thing main does is the
// screen-door fade — discarding a fraction of fragments proportional to (1 - shadowFade) via an
// interleaved-gradient-noise threshold, so a caster below the screen-size threshold thins its shadow
// out smoothly (PCF softens the holes) and vanishes at fade 0. Full-shadow casters (fade >= 1) keep
// every fragment.
layout(location = 0) in flat float v_shadow_fade;

void main()
{
    if (tbx_screen_door(v_shadow_fade, gl_FragCoord.xy))
        discard;
}
