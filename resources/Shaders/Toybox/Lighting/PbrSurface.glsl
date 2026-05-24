#ifndef TBX_PBR_SURFACE_GLSL
#define TBX_PBR_SURFACE_GLSL

struct PbrSurface
{
    vec3 world_position;
    vec3 normal;
    vec3 albedo;
    vec3 emissive;
    float alpha;
    float metallic;
    float roughness;
    float ao;
};

#endif
