#include "Base/UniversalShaderBase.glsl"

layout(location = 0) in vec3 v_sky_direction;
layout(location = 0) out vec4 o_color;

void main()
{
    // Interpolated cube/sphere directions drift off unit length; normalize before equirect lookup.
    vec3 direction = normalize(v_sky_direction);
    vec2 uv = vec2(
        atan(direction.z, direction.x) * TBX_INV_TAU + 0.5,
        asin(clamp(direction.y, -1.0, 1.0)) * TBX_INV_PI + 0.5);
    o_color = tbx_sample_global_texture(0u, uv);
}
