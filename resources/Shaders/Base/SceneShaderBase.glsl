#include "UniversalShaderBase.glsl"

struct EntityData
{
    mat4 modelMatrix;
    vec4 boundingSphere;
    uint meshId;
    uint materialId;
    uint padding0;
    uint padding1;
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_ENTITIES) readonly buffer GlobalEntitiesBuffer
{
    EntityData entities[];
};

layout(std430, binding = TBX_SHADER_BINDING_VISIBLE_ENTITY_IDS) buffer InstanceVisibilityBuffer
{
    uint visibleEntityIds[];
};

layout(std140, binding = TBX_SHADER_BINDING_SCENE_UNIFORMS) uniform SceneUniforms
{
    mat4 viewProjection;
    vec4 frustumPlanes[6];
    uint currentPassFilter;
    uint totalEntityCount;
};

bool tbx_is_sphere_visible(vec4 sphere)
{
    // CPU extraction normalizes the planes, so sphere.w can be compared as a world-space radius.
    for (int i = 0; i < 6; ++i)
    {
        if (dot(frustumPlanes[i].xyz, sphere.xyz) + frustumPlanes[i].w < -sphere.w)
        {
            return false;
        }
    }

    return true;
}

EntityData tbx_load_visible_entity(uint visible_index)
{
    return entities[visibleEntityIds[visible_index]];
}
