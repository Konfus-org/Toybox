#include "tbx/systems/graphics/resource_manager.h"
#include "systems/graphics/internal/resource_manager_internal.h"
#include <algorithm>
#include <utility>

namespace tbx
{
    RenderingResourceManager::RenderingResourceManager(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        const float resource_unload_time_seconds)
        : _backend(backend)
        , _resource_unload_time_seconds(std::max(0.0F, resource_unload_time_seconds))
        , _tracker(std::make_unique<internal::RenderingResourceTracker>())
        , _uploader(
              std::make_unique<internal::RenderingResourceUploader>(
                  std::move(backend),
                  std::move(asset_manager)))
    {
    }

    RenderingResourceManager::~RenderingResourceManager() = default;

    void RenderingResourceManager::update(const DeltaTime delta)
    {
        _tracker->update(delta);

        const auto backend = _backend.lock();
        if (!backend)
            return;

        const auto tracked_resources = _tracker->get_tracked_resources();
        for (const uint resource : tracked_resources)
        {
            if (_tracker->get_time_alive(resource) < _resource_unload_time_seconds)
                continue;

            const auto resource_id = Uuid(resource);
            if (backend->unload(resource_id))
            {
                _uploader->discard_cached_resource(resource_id);
                _tracker->untrack(resource);
            }
        }
    }

    bool RenderingResourceManager::is_managed(const Uuid& resource) const
    {
        return resource.is_valid() && _tracker->is_tracked(resource);
    }

    Result RenderingResourceManager::upload_dynamic_mesh(
        const std::shared_ptr<DynamicMeshData>& mesh_data,
        RenderingMeshUploadData& out_mesh) const
    {
        return _uploader->upload_dynamic_mesh(mesh_data, *_tracker, out_mesh);
    }

    Result RenderingResourceManager::upload_fallback_material(
        RenderingMaterialUploadData& out_material) const
    {
        return _uploader->upload_fallback_material(*_tracker, out_material);
    }

    Result RenderingResourceManager::upload_fallback_mesh(
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        return _uploader->upload_fallback_mesh(*_tracker, out_meshes);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_fallback_texture(
        const uint32 binding_id) const
    {
        return _uploader->upload_fallback_texture(*_tracker, binding_id);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_instance_buffer(
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        return _uploader
            ->upload_instance_buffer(*_tracker, cache_key, frame_index, data, byte_size);
    }

    Result RenderingResourceManager::upload_material(
        const MaterialInstance& instance,
        RenderingMaterialUploadData& out_material) const
    {
        return _uploader->upload_material(instance, *_tracker, out_material);
    }

    Result RenderingResourceManager::upload_model(
        const Handle& model_handle,
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        return _uploader->upload_model_meshes(model_handle, *_tracker, out_meshes);
    }

    Result RenderingResourceManager::upload_dynamic_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingMeshUploadData& out_mesh) const
    {
        return _uploader->upload_runtime_mesh(mesh_handle, mesh, *_tracker, out_mesh);
    }

    Result RenderingResourceManager::upload_static_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingMeshUploadData& out_mesh) const
    {
        return _uploader->upload_static_runtime_mesh(mesh_handle, mesh, *_tracker, out_mesh);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_texture(
        const uint32 slot,
        const std::string& cache_key,
        const GraphicsTextureDesc& desc) const
    {
        return _uploader->upload_texture(*_tracker, slot, cache_key, desc);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_uniform_buffer(
        const uint32 slot,
        const std::string& debug_name,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        return _uploader->upload_uniform_buffer(
            *_tracker,
            slot,
            debug_name,
            cache_key,
            frame_index,
            data,
            byte_size);
    }
}
