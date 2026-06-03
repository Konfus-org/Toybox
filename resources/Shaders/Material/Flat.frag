#include "Base/MaterialShaderBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 1) in vec4 v_color;
layout(location = 2) flat in uint v_material_id;

layout(location = 0) out vec4 o_color;

void main()
{
    o_color = tbx_sample_material_color(v_material_id, v_tex_coord, v_color);
}
