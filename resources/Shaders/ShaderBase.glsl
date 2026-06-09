#version 460 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shader_viewport_layer_array : enable
#extension GL_ARB_bindless_texture : require

// Buffer binding slots (SSBO/UBO space). KEEP IN SYNC with GPU_BINDING_* in shader_bindings.h.
#define TBX_SHADER_BINDING_ALL_INSTANCES 0
#define TBX_SHADER_BINDING_GLOBAL_VERTICES 1
#define TBX_SHADER_BINDING_GLOBAL_MESHES 2
#define TBX_SHADER_BINDING_GLOBAL_MATERIALS 3
#define TBX_SHADER_BINDING_GLOBAL_LIGHTS 4
#define TBX_SHADER_BINDING_CLUSTER_GRID 5
#define TBX_SHADER_BINDING_LIGHT_INDEX_POOL 6
#define TBX_SHADER_BINDING_MAIN_SCENE_DRAW_ARGS 7
#define TBX_SHADER_BINDING_SHADOW_ARGS_POOL 8
#define TBX_SHADER_BINDING_MAIN_SCENE_DRAW_COUNT 9
#define TBX_SHADER_BINDING_SHADOW_DRAW_COUNT 10
#define TBX_SHADER_BINDING_GLOBAL_TEXTURES 11

#define TBX_SHADER_BINDING_SCENE_UNIFORMS 0
#define TBX_SHADER_BINDING_GBUFFER_ALBEDO 17
#define TBX_SHADER_BINDING_GBUFFER_ROUGHNESS 18
#define TBX_SHADER_BINDING_GBUFFER_NORMAL 19
#define TBX_SHADER_BINDING_GBUFFER_METALLIC 20
#define TBX_SHADER_BINDING_SCENE_DEPTH 21
#define TBX_SHADER_BINDING_SHADOW_DEPTH_ATLAS 22
#define TBX_SHADER_BINDING_FINAL_HDR 23
#define TBX_SHADER_BINDING_POST_EFFECT_TEXTURE0 24
#define TBX_SHADER_BINDING_FINAL_HDR_IMAGE 0

#define TBX_BINDING_GBUFFER_FINAL_COLOR TBX_SHADER_BINDING_FINAL_HDR

#define TBX_SHADER_PIPELINE_FLAG_OPAQUE (1u << 0u)
#define TBX_SHADER_PIPELINE_FLAG_TRANSPARENT (1u << 1u)
#define TBX_SHADER_PIPELINE_FLAG_SHADOW (1u << 2u)

#define TBX_SHADER_LIGHT_TYPE_DIRECTIONAL 0u
#define TBX_SHADER_LIGHT_TYPE_POINT 1u
#define TBX_SHADER_LIGHT_TYPE_SPOT 2u
#define TBX_SHADER_LIGHT_TYPE_AREA 3u

#define TBX_MAX_GLOBAL_TEXTURES 16
#define TBX_MAX_LIGHTS_PER_CLUSTER 128u
#define TBX_MAX_SHADOWS 128u
#define TBX_EPSILON 0.00001
#define TBX_PI 3.14159265358979323846
#define TBX_INV_PI 0.31830988618379067154
#define TBX_INV_TAU 0.15915494309189533577

struct VertexData
{
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec4 uv;
    vec4 color;
};

struct MeshData
{
    uint firstIndex;
    uint indexCount;
    int baseVertex;
    uint vertexCount;
};

struct InstanceData
{
    mat4 modelMatrix;
    mat4 prevModelMatrix; // for motion vectors / TAA; matches GpuInstanceData
    vec4 boundsMin;
    vec4 boundsMax;
    uint meshId;
    uint materialId;
    uint padding0;
    uint padding1;
};

struct DrawCommand
{
    uint count;
    uint instanceCount;
    uint firstIndex;
    int baseVertex;
    uint baseInstance;
};

struct LightData
{
    vec4 positionRange;
    vec4 directionType;
    vec4 colorIntensity;
    vec4 spotAnglesArea;
    vec4 shadowData;
};

struct ClusterGridData
{
    uint lightOffset;
    uint lightCount;
    uint padding0;
    uint padding1;
};

// Bindless global texture table: each entry is a resident sampler2D handle (uint64) written by
// GpuSceneBuffers from get_texture_bindless_handle(), indexed by a material's texture index.
layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_TEXTURES) readonly buffer TbxGlobalTextureTable
{
    sampler2D globalTextures[];
};
layout(binding = TBX_SHADER_BINDING_GBUFFER_ALBEDO) uniform sampler2D tbx_gbuffer_albedo;
layout(binding = TBX_SHADER_BINDING_GBUFFER_ROUGHNESS) uniform sampler2D tbx_gbuffer_roughness;
layout(binding = TBX_SHADER_BINDING_GBUFFER_NORMAL) uniform sampler2D tbx_gbuffer_normal;
layout(binding = TBX_SHADER_BINDING_GBUFFER_METALLIC) uniform sampler2D tbx_gbuffer_metallic;
layout(binding = TBX_SHADER_BINDING_SCENE_DEPTH) uniform sampler2D tbx_scene_depth;
layout(binding = TBX_SHADER_BINDING_SHADOW_DEPTH_ATLAS) uniform sampler2DArray
    tbx_shadow_depth_atlas;
layout(binding = TBX_SHADER_BINDING_FINAL_HDR) uniform sampler2D tbx_gbuffer_final_color;
layout(binding = TBX_SHADER_BINDING_POST_EFFECT_TEXTURE0) uniform sampler2D
    tbx_post_effect_texture0;

layout(std140, binding = TBX_SHADER_BINDING_SCENE_UNIFORMS) uniform TbxSceneUniforms
{
    mat4 viewProjection;
    mat4 inverseViewProjection;
    vec4 frustumPlanes[6];
    vec4 ambientLight;
    vec4 cameraPositionTime;
    vec4 shadowSettings;
    vec4 skyColor;
    vec4 skyParams;
    vec4 screenSize;
    vec4 clusterDimensions;
    uint currentPassFilter;
    uint totalInstanceCount;
    uint totalMeshCount;
    uint totalMaterialCount;
    uint lightCount;
    uint shadowCount;
    uint maxSceneDrawCount;
    uint maxShadowDrawCount;
};

layout(std430, binding = TBX_SHADER_BINDING_ALL_INSTANCES) readonly buffer TbxAllInstancesBuffer
{
    InstanceData instances[];
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_VERTICES) readonly buffer TbxGlobalVertexBuffer
{
    VertexData vertices[];
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_MESHES) readonly buffer TbxGlobalMeshBuffer
{
    MeshData meshes[];
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_LIGHTS) readonly buffer TbxGlobalLightBuffer
{
    LightData lights[];
};

layout(std430, binding = TBX_SHADER_BINDING_CLUSTER_GRID) buffer TbxClusterGridBuffer
{
    ClusterGridData clusters[];
};

layout(std430, binding = TBX_SHADER_BINDING_LIGHT_INDEX_POOL) buffer TbxLightIndexPoolBuffer
{
    uint lightIndexPool[];
};

layout(std430, binding = TBX_SHADER_BINDING_MAIN_SCENE_DRAW_ARGS) buffer TbxMainSceneDrawArgs
{
    DrawCommand mainSceneDrawArgs[];
};

layout(std430, binding = TBX_SHADER_BINDING_SHADOW_ARGS_POOL) buffer TbxShadowArgsPool
{
    DrawCommand shadowArgsPool[];
};

layout(std430, binding = TBX_SHADER_BINDING_MAIN_SCENE_DRAW_COUNT) buffer TbxMainSceneDrawCount
{
    uint mainSceneDrawCount;
};

layout(std430, binding = TBX_SHADER_BINDING_SHADOW_DRAW_COUNT) buffer TbxShadowDrawCount
{
    uint shadowDrawCount;
};

float tbx_saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec2 tbx_saturate(vec2 value)
{
    return clamp(value, vec2(0.0), vec2(1.0));
}

vec3 tbx_saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec4 tbx_saturate(vec4 value)
{
    return clamp(value, vec4(0.0), vec4(1.0));
}

vec4 tbx_write_to_final_color(vec3 color, float alpha)
{
    return vec4(color, alpha);
}

vec4 tbx_sample_global_texture(uint texture_index, vec2 tex_coord)
{
    return texture(globalTextures[nonuniformEXT(texture_index)], tex_coord);
}

uint tbx_cluster_count()
{
    return uint(clusterDimensions.x * clusterDimensions.y * clusterDimensions.z);
}

bool tbx_aabb_is_inside_frustum(vec3 bounds_min, vec3 bounds_max)
{
    for (uint index = 0u; index < 6u; ++index)
    {
        vec3 positive =
            mix(bounds_min, bounds_max, greaterThanEqual(frustumPlanes[index].xyz, vec3(0.0)));
        if (dot(frustumPlanes[index].xyz, positive) + frustumPlanes[index].w < 0.0)
        {
            return false;
        }
    }
    return true;
}

vec3 tbx_reconstruct_world_position(vec2 uv, float depth)
{
    vec4 clip = vec4((uv * 2.0) - vec2(1.0), (depth * 2.0) - 1.0, 1.0);
    vec4 world = inverseViewProjection * clip;
    return world.xyz / max(abs(world.w), TBX_EPSILON);
}

vec3 tbx_safe_light_direction(LightData light)
{
    vec3 direction = light.directionType.xyz;
    if (dot(direction, direction) <= TBX_EPSILON)
    {
        return vec3(0.0, 0.0, -1.0);
    }

    return normalize(direction);
}

mat4 tbx_make_look_at(vec3 eye, vec3 center)
{
    vec3 forward = normalize(center - eye);
    vec3 up =
        abs(dot(forward, vec3(0.0, 1.0, 0.0))) > 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 side = normalize(cross(forward, up));
    vec3 corrected_up = cross(side, forward);
    return mat4(
        vec4(side.x, corrected_up.x, -forward.x, 0.0),
        vec4(side.y, corrected_up.y, -forward.y, 0.0),
        vec4(side.z, corrected_up.z, -forward.z, 0.0),
        vec4(-dot(side, eye), -dot(corrected_up, eye), dot(forward, eye), 1.0));
}

mat4 tbx_make_orthographic(float left, float right, float bottom, float top, float near, float far)
{
    return mat4(
        vec4(2.0 / (right - left), 0.0, 0.0, 0.0),
        vec4(0.0, 2.0 / (top - bottom), 0.0, 0.0),
        vec4(0.0, 0.0, -2.0 / (far - near), 0.0),
        vec4(
            -(right + left) / (right - left),
            -(top + bottom) / (top - bottom),
            -(far + near) / (far - near),
            1.0));
}

mat4 tbx_make_perspective(float fov_radians, float aspect, float near, float far)
{
    float tan_half_fov = tan(fov_radians * 0.5);
    return mat4(
        vec4(1.0 / (aspect * tan_half_fov), 0.0, 0.0, 0.0),
        vec4(0.0, 1.0 / tan_half_fov, 0.0, 0.0),
        vec4(0.0, 0.0, -(far + near) / (far - near), -1.0),
        vec4(0.0, 0.0, -(2.0 * far * near) / (far - near), 0.0));
}

mat4 tbx_make_light_view_projection(LightData light)
{
    uint light_type = uint(light.directionType.w);
    vec3 direction = tbx_safe_light_direction(light);
    vec3 position = light.positionRange.xyz;
    vec3 target = position;
    vec3 eye = position;
    if (light_type == TBX_SHADER_LIGHT_TYPE_DIRECTIONAL)
    {
        float render_distance = max(shadowSettings.y, 1.0);
        float caster_distance = max(shadowSettings.w, render_distance);
        float half_render_distance = render_distance * 0.5;
        target = cameraPositionTime.xyz;
        eye = target - (direction * (caster_distance * 0.5));
        mat4 view = tbx_make_look_at(eye, target);
        return tbx_make_orthographic(
                   -half_render_distance,
                   half_render_distance,
                   -half_render_distance,
                   half_render_distance,
                   0.1,
                   caster_distance)
               * view;
    }

    mat4 view = tbx_make_look_at(eye, eye + direction);

    float fov_degrees = light_type == TBX_SHADER_LIGHT_TYPE_SPOT
                            ? clamp(light.spotAnglesArea.y * 2.0, 1.0, 179.0)
                            : 90.0;
    return tbx_make_perspective(radians(fov_degrees), 1.0, 0.1, max(light.positionRange.w, 1.0))
           * view;
}

void tbx_write_draw_command(
    uint slot,
    uint instance_id,
    uint encoded_base_instance,
    bool write_shadow_command)
{
    InstanceData instance = instances[instance_id];
    MeshData mesh = meshes[instance.meshId];
    DrawCommand command =
        DrawCommand(mesh.indexCount, 1u, mesh.firstIndex, mesh.baseVertex, encoded_base_instance);

    if (write_shadow_command)
    {
        shadowArgsPool[slot] = command;
        return;
    }

    mainSceneDrawArgs[slot] = command;
}
