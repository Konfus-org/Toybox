#include "Toybox/ShaderBase.glsl"

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxUnlitMaterialData
{
    vec4 u_albedo_color;
};

layout(binding = TBX_BINDING_ALBEDO_MAP) uniform sampler2D u_albedo_map;

vec4 tbx_build_unlit_color(vec2 uv)
{
    return texture(u_albedo_map, uv) * u_albedo_color;
}
