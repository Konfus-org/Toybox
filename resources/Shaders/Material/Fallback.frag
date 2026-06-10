#include "ShaderBase.glsl"

// Validation surface: the unlit, pulsating debug surface every failed resource falls back to. The
// per-failure debug color comes from material param lane 0 (RenderValidation sets it), and texture
// slot 0 is multiplied in so the texture-failure material's checkerboard shows through; other
// failures bind no texture and render a solid color. Unaffected by scene lighting.
layout(location = 0) in vec2 v_uv;
layout(location = 5) in flat uint v_material_id;
layout(location = 0) out vec4 o_color;

void main()
{
    float pulse = 0.55 + (0.45 * (0.5 + (0.5 * sin(cameraPositionTime.w * 4.0))));
    vec3 color = tbx_material_param(v_material_id, 0u).rgb;
    vec3 checker = tbx_sample_material_texture(v_material_id, 0u, v_uv, vec4(1.0)).rgb;
    o_color = vec4(color * checker * pulse, 1.0);
}
