#include "ShaderBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 1) in vec4 v_color;
layout(location = 2) flat in uint v_material_id;

layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec4 o_roughness;
layout(location = 2) out vec4 o_normal;
layout(location = 3) out vec4 o_metallic;

void main()
{
    MaterialData material = materials[v_material_id];
    vec4 color = tbx_sample_material_color(v_material_id, v_tex_coord) * v_color;
    if (material.alphaCutoff > 0.0 && color.a < material.alphaCutoff)
    {
        discard;
    }
    tbx_write_gbuffer(
        color.rgb,
        material.surface.y,
        vec3(0.0, 0.0, 1.0),
        material.surface.x,
        o_albedo,
        o_roughness,
        o_normal,
        o_metallic);
}
