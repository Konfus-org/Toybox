#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/mesh_bounds.h"

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
    /// Purpose: Owns rendering upload and tracking helpers behind one resource API.
    /// @details
    /// Ownership: Owns upload caches through the uploader and owns resource last-use tracking.
    /// Thread Safety: Not inherently thread-safe; call from the render lane.
    class TBX_API RenderingResourceManager final
    {
      public:
        RenderingResourceManager(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            float resource_unload_time_seconds = 3.0F);
        ~RenderingResourceManager();

      public:
        RenderingResourceManager(const RenderingResourceManager&) = delete;
        RenderingResourceManager& operator=(const RenderingResourceManager&) = delete;
        RenderingResourceManager(RenderingResourceManager&&) noexcept = delete;
        RenderingResourceManager& operator=(RenderingResourceManager&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Advances managed resource ages and unloads stale resources.
        void update(DeltaTime delta);

        /// @brief
        /// Purpose: Returns true when a backend resource is managed by this manager.
        bool is_managed(const Uuid& resource) const;

        /// @brief
        /// Purpose: Removes cached upload records affected by a reloaded asset.
        void invalidate_asset(const Handle& asset) const;

        /// @brief
        /// Purpose: Caches static model bounds used by frame extraction culling.
        void cache_model_bounds(const Handle& model_handle, const MeshBounds& bounds) const;

        /// @brief
        /// Purpose: Sets the age threshold used to unload unused managed resources.
        void set_resource_unload_time_seconds(float seconds);

        /// @brief
        /// Purpose: Looks up cached static model bounds used by frame extraction culling.
        bool try_get_model_bounds(const Handle& model_handle, MeshBounds& out_bounds) const;

        /// @brief
        /// Purpose: Returns the age threshold used to unload unused managed resources.
        float get_resource_unload_time_seconds() const;

        /// @brief
        /// Purpose: Uploads, updates, or reuses one shared dynamic runtime mesh buffer pair.
        RenderingMeshUploadData upload_dynamic_mesh(
            const std::shared_ptr<DynamicMeshData>& mesh_data) const;

        /// @brief
        /// Purpose: Uploads or reuses one bind group for ordered draw resource bindings.
        Uuid upload_bind_group(const BindGroupDesc& desc) const;

        /// @brief
        /// Purpose: Uploads the renderer fallback material used for incomplete draw data.
        RenderingMaterialUploadData upload_fallback_material() const;

        /// @brief
        /// Purpose: Uploads a fallback mesh when extracted geometry cannot produce commands.
        std::vector<RenderingMeshUploadData> upload_fallback_mesh() const;

        /// @brief
        /// Purpose: Uploads a fallback texture for a material texture binding id.
        GraphicsResourceBinding upload_fallback_texture(uint32 binding_id) const;

        /// @brief
        /// Purpose: Uploads one frame-local instance transform vertex buffer.
        GraphicsResourceBinding upload_instance_buffer(
            const std::string& cache_key,
            uint64 frame_index,
            const void* data,
            uint64 byte_size) const;

        /// @brief
        /// Purpose: Uploads a material pipeline, material uniforms, and material textures.
        RenderingMaterialUploadData upload_material(const MaterialInstance& instance) const;

        /// @brief
        /// Purpose: Uploads all mesh buffers for one model asset.
        std::vector<RenderingMeshUploadData> upload_model(const Handle& model_handle) const;

        /// @brief
        /// Purpose: Uploads one runtime mesh buffer pair.
        RenderingMeshUploadData upload_dynamic_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh) const;

        /// @brief
        /// Purpose: Uploads or reuses one stable runtime mesh buffer pair.
        RenderingMeshUploadData upload_static_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh) const;

        /// @brief
        /// Purpose: Uploads or reuses a renderer-owned texture such as a pass target.
        GraphicsResourceBinding upload_texture(
            uint32 slot,
            const std::string& cache_key,
            const GraphicsTextureDesc& desc) const;

        /// @brief
        /// Purpose: Uploads or updates one frame-versioned uniform buffer.
        GraphicsResourceBinding upload_uniform_buffer(
            uint32 slot,
            const std::string& debug_name,
            const std::string& cache_key,
            uint64 frame_index,
            const void* data,
            uint64 byte_size) const;

      private:
        struct State;
        std::unique_ptr<State> _state = {};
    };
}
