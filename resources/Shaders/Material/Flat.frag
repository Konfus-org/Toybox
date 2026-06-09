layout(location = 0) in vec2 v_tex_coord;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 o_color;

layout(std140, binding = 2) uniform TbxAlbedoColor
{
    vec4 albedoColor;
};

layout(binding = 16) uniform sampler2D albedoMap;

void main()
{
    o_color = albedoColor * texture(albedoMap, v_tex_coord) * v_color;
}
