#include "ShaderBase.glsl"

layout(location = 0) in vec2 v_tex_coord;
layout(location = 1) in vec4 v_color;
layout(location = 2) flat in uint v_material_id;
layout(location = 3) in vec3 v_world_position;
layout(location = 4) in vec3 v_world_normal;
layout(location = 5) in vec4 v_world_tangent;

layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec4 o_roughness;
layout(location = 2) out vec4 o_normal;
layout(location = 3) out vec4 o_metallic;

void main()
{
    vec4 material_color = tbx_sample_material_color(v_material_id, v_tex_coord);
    MaterialData material = materials[v_material_id];
    if (material.alphaCutoff > 0.0 && material_color.a < material.alphaCutoff)
    {
        discard;
    }
    vec3 normal = tbx_resolve_normal(v_material_id, v_tex_coord, v_world_normal, v_world_tangent);
    float metallic = tbx_sample_material_metallic(v_material_id, v_tex_coord);
    float roughness = tbx_sample_material_roughness(v_material_id, v_tex_coord);
    tbx_write_gbuffer(
        material_color.rgb * v_color.rgb,
        roughness,
        normal,
        metallic,
        o_albedo,
        o_roughness,
        o_normal,
        o_metallic);
}
