#pragma once
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/mesh_bounds.h"

namespace tbx
{
    /// @brief
    /// Purpose: Stores one cached uniform buffer ring entry.
    struct TBX_API UniformBufferCacheEntry
    {
        Uuid resource = {};
        uint64 byte_size = 0U;
    };

    /// @brief
    /// Purpose: Caches uploaded pipeline resources by material configuration key.
    struct TBX_API PipelineResourceCache
    {
        std::unordered_map<std::string, Uuid> pipelines = {};
    };

    /// @brief
    /// Purpose: Caches uploaded dynamic mesh resources by shared runtime mesh payload.
    struct TBX_API DynamicMeshResourceCacheEntry
    {
        std::weak_ptr<DynamicMeshData> data = {};
        RenderingMeshUploadData mesh = {};
    };

    /// @brief
    /// Purpose: Caches uploaded mesh resources by source mesh handle.
    struct TBX_API MeshResourceCache
    {
        std::unordered_map<Handle, MeshBounds> model_bounds = {};
        std::unordered_map<Handle, std::vector<RenderingMeshUploadData>> model_meshes = {};
        std::unordered_map<Handle, RenderingMeshUploadData> runtime_meshes = {};
        std::unordered_map<const DynamicMeshData*, DynamicMeshResourceCacheEntry> dynamic_meshes =
            {};
    };

    /// @brief
    /// Purpose: Caches uploaded texture resources by asset handle or generated default key.
    struct TBX_API TextureResourceCache
    {
        std::unordered_map<Handle, Uuid> textures = {};
        std::unordered_map<uint32, Uuid> default_textures = {};
        std::unordered_map<std::string, Uuid> render_targets = {};
    };

    /// @brief
    /// Purpose: Caches frame-versioned uniform buffer rings by semantic upload key.
    struct TBX_API UniformBufferCache
    {
        std::unordered_map<std::string, std::vector<UniformBufferCacheEntry>> uniform_buffers = {};
    };

    /// @brief
    /// Purpose: Stores one cached bind group and its canonical binding description.
    struct TBX_API RenderingBindGroupCacheEntry
    {
        Uuid resource = {};
        BindGroupDesc desc = {};
    };

    /// @brief
    /// Purpose: Caches uploaded bind groups by ordered resource binding content.
    struct TBX_API RenderingBindGroupCache
    {
        std::unordered_map<uint64, std::vector<RenderingBindGroupCacheEntry>> bind_groups = {};
    };

    /// @brief
    /// Purpose: Caches resolved material upload data by material instance content.
    struct TBX_API MaterialResourceCache
    {
        std::unordered_map<uint64, RenderingMaterialUploadData> materials = {};
    };

    /// @brief
    /// Purpose: Groups renderer upload caches owned by the resource manager internals.
    struct TBX_API ResourceUploadCaches
    {
        RenderingBindGroupCache bind_groups = {};
        MaterialResourceCache materials = {};
        PipelineResourceCache pipelines = {};
        MeshResourceCache meshes = {};
        TextureResourceCache textures = {};
        UniformBufferCache uniforms = {};
        UniformBufferCache instances = {};
    };
}
