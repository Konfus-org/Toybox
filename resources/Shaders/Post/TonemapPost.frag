#include "ShaderBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;

void main()
{
    vec3 hdr = texture(tbx_scene_color, v_tex_coord).rgb;
    vec3 mapped = hdr / (hdr + vec3(1.0));
    // Clamp before gamma so negative HDR artifacts cannot produce NaNs.
    o_color = vec4(pow(tbx_saturate(mapped), vec3(1.0 / 2.2)), 1.0);
}
