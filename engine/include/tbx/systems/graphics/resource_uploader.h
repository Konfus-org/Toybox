#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/resource_tracker.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/handle.h"
#include "tbx/types/material.h"
#include "tbx/types/matrices.h"
#include "tbx/utils/result.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Stores uploaded material resources needed by a draw command.
    /// @details
    /// Ownership: Owns backend resource identifiers and copied CPU uniform values.
    /// Thread Safety: Safe to move between render pipeline stages.
    struct TBX_API RenderingMaterialUploadData
    {
        Uuid pipeline = {};
        std::vector<Vec4> uniform_values = {};
        std::vector<GraphicsResourceBinding> textures = {};
    };

    /// @brief
    /// Purpose: Stores uploaded mesh resources needed by an indexed draw command.
    /// @details
    /// Ownership: Owns backend resource identifiers by value.
    /// Thread Safety: Safe to move between render pipeline stages.
    struct TBX_API RenderingMeshUploadData
    {
        Uuid vertex_buffer = {};
        Uuid index_buffer = {};
        uint32 index_count = 0U;
        uint64 vertex_byte_size = 0U;
        uint64 index_byte_size = 0U;
    };

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
        std::unordered_map<Handle, std::vector<RenderingMeshUploadData>> model_meshes = {};
        std::unordered_map<Handle, RenderingMeshUploadData> runtime_meshes = {};
        std::unordered_map<const DynamicMeshData*, DynamicMeshResourceCacheEntry>
            dynamic_meshes = {};
    };

    /// @brief
    /// Purpose: Caches uploaded texture resources by asset handle or generated default key.
    struct TBX_API TextureResourceCache
    {
        std::unordered_map<Handle, Uuid> textures = {};
        std::unordered_map<std::string, Uuid> default_textures = {};
        std::unordered_map<std::string, Uuid> render_targets = {};
    };

    /// @brief
    /// Purpose: Caches frame-versioned uniform buffer rings by semantic upload key.
    struct TBX_API UniformBufferCache
    {
        std::unordered_map<std::string, std::vector<UniformBufferCacheEntry>> uniform_buffers = {};
    };

    /// @brief
    /// Purpose: Groups renderer upload caches while keeping lifetime policy in the tracker.
    struct TBX_API ResourceUploadCaches
    {
        PipelineResourceCache pipelines = {};
        MeshResourceCache meshes = {};
        TextureResourceCache textures = {};
        UniformBufferCache uniforms = {};
        UniformBufferCache instances = {};
    };

    /// @brief
    /// Purpose: Uploads rendering data directly through the graphics backend.
    /// @details
    /// Ownership: Borrows backend and asset services. Caches stable asset-backed GPU resources;
    /// frame-local uniform data remains uploaded per frame.
    /// Thread Safety: Not inherently thread-safe; call from the render lane.
    class TBX_API ResourceUploader final
    {
      public:
        ResourceUploader(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager);
        ~ResourceUploader() = default;

      public:
        ResourceUploader(const ResourceUploader&) = delete;
        ResourceUploader& operator=(const ResourceUploader&) = delete;
        ResourceUploader(ResourceUploader&&) noexcept = delete;
        ResourceUploader& operator=(ResourceUploader&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Uploads a fallback mesh when extracted geometry cannot produce commands.
        Result upload_fallback_mesh(
            RenderingResourceTracker& resource_tracker,
            std::vector<RenderingMeshUploadData>& out_meshes) const;

        /// @brief
        /// Purpose: Uploads a material pipeline, material uniforms, and material textures.
        Result upload_material(
            const MaterialInstance& instance,
            RenderingResourceTracker& resource_tracker,
            RenderingMaterialUploadData& out_material) const;

        /// @brief
        /// Purpose: Uploads, updates, or reuses one shared dynamic runtime mesh buffer pair.
        Result upload_dynamic_mesh(
            const std::shared_ptr<DynamicMeshData>& mesh_data,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        /// @brief
        /// Purpose: Uploads all mesh buffers for one model asset.
        Result upload_model_meshes(
            const Handle& model_handle,
            RenderingResourceTracker& resource_tracker,
            std::vector<RenderingMeshUploadData>& out_meshes) const;

        /// @brief
        /// Purpose: Uploads one runtime mesh buffer pair.
        Result upload_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        /// @brief
        /// Purpose: Reuses one cached stable runtime mesh buffer pair.
        bool try_get_static_runtime_mesh(
            const Handle& mesh_handle,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        /// @brief
        /// Purpose: Returns true when a stable runtime mesh already has cached GPU buffers.
        bool has_static_runtime_mesh(const Handle& mesh_handle) const;

        /// @brief
        /// Purpose: Uploads or reuses one stable runtime mesh buffer pair.
        Result upload_static_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        /// @brief
        /// Purpose: Uploads one frame-local instance transform vertex buffer.
        GraphicsResourceBinding upload_instance_buffer(
            RenderingResourceTracker& resource_tracker,
            const std::string& cache_key,
            uint64 frame_index,
            const void* data,
            uint64 byte_size) const;

        /// @brief
        /// Purpose: Uploads or updates one frame-versioned uniform buffer.
        GraphicsResourceBinding upload_uniform_buffer(
            RenderingResourceTracker& resource_tracker,
            uint32 slot,
            const std::string& debug_name,
            const std::string& cache_key,
            uint64 frame_index,
            const void* data,
            uint64 byte_size) const;

        /// @brief
        /// Purpose: Uploads or reuses a renderer-owned texture such as a pass target.
        GraphicsResourceBinding upload_texture(
            RenderingResourceTracker& resource_tracker,
            uint32 slot,
            const std::string& cache_key,
            const GraphicsTextureDesc& desc) const;

        /// @brief
        /// Purpose: Removes any cached upload entry that references an unloaded backend resource.
        void discard_cached_resource(const Uuid& resource);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        mutable ResourceUploadCaches _caches = {};
    };
}
