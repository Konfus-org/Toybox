layout(std140, binding = 0) uniform ToyboxViewBlock
{
    mat4 u_view_proj;
};

layout(std140, binding = 1) uniform ToyboxMaterialBlock
{
    vec4 u_material_uniforms[64];
};