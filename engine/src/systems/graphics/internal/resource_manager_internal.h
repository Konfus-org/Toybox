#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/resource_upload_caches.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/components/model.h"
#include "tbx/types/shader.h"
#include "tbx/types/texture.h"
#include "tbx/types/typedefs.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx::internal
{
    class RenderingResourceTracker final
    {
      public:
        using ResourceCollection = std::vector<uint>;

      public:
        RenderingResourceTracker() = default;
        ~RenderingResourceTracker() = default;

      public:
        RenderingResourceTracker(const RenderingResourceTracker&) = delete;
        RenderingResourceTracker& operator=(const RenderingResourceTracker&) = delete;
        RenderingResourceTracker(RenderingResourceTracker&&) noexcept = default;
        RenderingResourceTracker& operator=(RenderingResourceTracker&&) noexcept = default;

      public:
        const ResourceCollection& get_tracked_resources() const;

        float get_time_alive(uint resource) const;

        bool is_tracked(uint resource) const;

        void track(uint resource);

        void untrack(uint resource);

        void update(DeltaTime delta);

      private:
        ResourceCollection _tracked_resources = {};
        std::unordered_map<uint, float> _time_alive = {};
    };

    class RenderingResourceUploader final
    {
      public:
        RenderingResourceUploader(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager);
        ~RenderingResourceUploader() = default;

      public:
        RenderingResourceUploader(const RenderingResourceUploader&) = delete;
        RenderingResourceUploader& operator=(const RenderingResourceUploader&) = delete;
        RenderingResourceUploader(RenderingResourceUploader&&) noexcept = delete;
        RenderingResourceUploader& operator=(RenderingResourceUploader&&) noexcept = delete;

      public:
        void discard_cached_resource(const Uuid& resource);

        void cache_model_bounds(const Handle& model_handle, const MeshBounds& bounds) const;

        Result upload_dynamic_mesh(
            const std::shared_ptr<DynamicMeshData>& mesh_data,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        Result upload_fallback_mesh(
            RenderingResourceTracker& resource_tracker,
            std::vector<RenderingMeshUploadData>& out_meshes) const;

        Result upload_fallback_material(
            RenderingResourceTracker& resource_tracker,
            RenderingMaterialUploadData& out_material) const;

        GraphicsResourceBinding upload_fallback_texture(
            RenderingResourceTracker& resource_tracker,
            uint32 binding_id) const;

        GraphicsResourceBinding upload_instance_buffer(
            RenderingResourceTracker& resource_tracker,
            const std::string& cache_key,
            uint64 frame_index,
            const void* data,
            uint64 byte_size) const;

        Result upload_material(
            const MaterialInstance& instance,
            RenderingResourceTracker& resource_tracker,
            RenderingMaterialUploadData& out_material) const;

        Result upload_model_meshes(
            const Handle& model_handle,
            RenderingResourceTracker& resource_tracker,
            std::vector<RenderingMeshUploadData>& out_meshes) const;

        Result upload_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        Result upload_static_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        GraphicsResourceBinding upload_texture(
            RenderingResourceTracker& resource_tracker,
            uint32 slot,
            const std::string& cache_key,
            const GraphicsTextureDesc& desc) const;

        GraphicsResourceBinding upload_uniform_buffer(
            RenderingResourceTracker& resource_tracker,
            uint32 slot,
            const std::string& debug_name,
            const std::string& cache_key,
            uint64 frame_index,
            const void* data,
            uint64 byte_size) const;

        bool try_get_static_runtime_mesh(
            const Handle& mesh_handle,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const;

        bool try_get_model_bounds(const Handle& model_handle, MeshBounds& out_bounds) const;

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        mutable ResourceUploadCaches _caches = {};
        std::shared_ptr<Material> _fallback_material = {};
        std::shared_ptr<Model> _fallback_model = {};
        std::shared_ptr<ShaderProgram> _fallback_shader = {};
        mutable std::unordered_map<uint32, Texture> _fallback_textures = {};
    };
}
