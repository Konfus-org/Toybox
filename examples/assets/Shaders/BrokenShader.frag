#include "ShaderBase.glsl"

// Intentionally broken fragment shader used by the example scene to exercise the renderer's
// shader-compile-failure fallback (it must fall back to the magenta surface and warn once).
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

void main()
{
    o_color = this_symbol_does_not_exist_on_purpose;
}
