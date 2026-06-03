#include "SceneShaderBase.glsl"

struct MaterialData
{
    uint pipelineFlags;
    uint textureIndex;
    uint padding0;
    uint padding1;
    vec4 baseColor;
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_MATERIALS) readonly buffer GlobalMaterialsBuffer
{
    MaterialData materials[];
};

vec4 tbx_sample_material_color(uint material_id, vec2 tex_coord, vec4 vertex_color)
{
    MaterialData material = materials[material_id];
    return tbx_sample_global_texture(material.textureIndex, tex_coord) * material.baseColor * vertex_color;
}
