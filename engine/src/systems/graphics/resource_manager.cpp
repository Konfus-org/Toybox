#include "tbx/systems/graphics/resource_manager.h"
#include "systems/graphics/internal/resource_manager_internal.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace tbx
{
    struct RenderingResourceManager::Impl
    {
        Impl(
            std::weak_ptr<IGraphicsBackend> graphics_backend,
            std::weak_ptr<AssetManager> asset_manager,
            const float unload_time_seconds)
            : backend(graphics_backend)
            , tracker(std::make_unique<internal::RenderingResourceTracker>())
            , uploader(
                  std::make_unique<internal::RenderingResourceUploader>(
                      std::move(graphics_backend),
                      std::move(asset_manager)))
            , resource_unload_time_seconds(std::max(0.0F, unload_time_seconds))
        {
        }

        std::weak_ptr<IGraphicsBackend> backend = {};
        std::unique_ptr<internal::RenderingResourceTracker> tracker = {};
        std::unique_ptr<internal::RenderingResourceUploader> uploader = {};
        float resource_unload_time_seconds = 3.0F;
    };

    RenderingResourceManager::RenderingResourceManager(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        const float resource_unload_time_seconds)
        : _impl(
              std::make_unique<Impl>(
                  std::move(backend),
                  std::move(asset_manager),
                  resource_unload_time_seconds))
    {
    }

    RenderingResourceManager::~RenderingResourceManager() = default;

    void RenderingResourceManager::update(const DeltaTime delta)
    {
        _impl->tracker->update(delta);

        const auto backend = _impl->backend.lock();
        if (!backend)
            return;

        auto stale_resources = std::vector<uint>();
        const auto& tracked_resources = _impl->tracker->get_tracked_resources();
        for (const uint resource : tracked_resources)
        {
            if (_impl->tracker->get_time_alive(resource) < _impl->resource_unload_time_seconds)
                continue;

            stale_resources.push_back(resource);
        }

        for (const uint resource : stale_resources)
        {
            const auto resource_id = Uuid(resource);
            if (backend->destroy_resource(resource_id))
            {
                _impl->uploader->discard_cached_resource(resource_id);
                _impl->tracker->untrack(resource);
            }
        }
    }

    bool RenderingResourceManager::is_managed(const Uuid& resource) const
    {
        return resource.is_valid() && _impl->tracker->is_tracked(resource);
    }

    void RenderingResourceManager::cache_model_bounds(
        const Handle& model_handle,
        const MeshBounds& bounds) const
    {
        _impl->uploader->cache_model_bounds(model_handle, bounds);
    }

    bool RenderingResourceManager::try_get_model_bounds(
        const Handle& model_handle,
        MeshBounds& out_bounds) const
    {
        return _impl->uploader->try_get_model_bounds(model_handle, out_bounds);
    }

    Result RenderingResourceManager::upload_dynamic_mesh(
        const std::shared_ptr<DynamicMeshData>& mesh_data,
        RenderingMeshUploadData& out_mesh) const
    {
        return _impl->uploader->upload_dynamic_mesh(mesh_data, *_impl->tracker, out_mesh);
    }

    Result RenderingResourceManager::upload_bind_group(
        const BindGroupDesc& desc,
        Uuid& out_bind_group) const
    {
        return _impl->uploader->upload_bind_group(desc, *_impl->tracker, out_bind_group);
    }

    Result RenderingResourceManager::upload_fallback_material(
        RenderingMaterialUploadData& out_material) const
    {
        return _impl->uploader->upload_fallback_material(*_impl->tracker, out_material);
    }

    Result RenderingResourceManager::upload_fallback_mesh(
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        return _impl->uploader->upload_fallback_mesh(*_impl->tracker, out_meshes);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_fallback_texture(
        const uint32 binding_id) const
    {
        return _impl->uploader->upload_fallback_texture(*_impl->tracker, binding_id);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_instance_buffer(
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        return _impl->uploader
            ->upload_instance_buffer(*_impl->tracker, cache_key, frame_index, data, byte_size);
    }

    Result RenderingResourceManager::upload_material(
        const MaterialInstance& instance,
        RenderingMaterialUploadData& out_material) const
    {
        return _impl->uploader->upload_material(instance, *_impl->tracker, out_material);
    }

    Result RenderingResourceManager::upload_model(
        const Handle& model_handle,
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        return _impl->uploader->upload_model_meshes(model_handle, *_impl->tracker, out_meshes);
    }

    Result RenderingResourceManager::upload_dynamic_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingMeshUploadData& out_mesh) const
    {
        return _impl->uploader->upload_runtime_mesh(mesh_handle, mesh, *_impl->tracker, out_mesh);
    }

    Result RenderingResourceManager::upload_static_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingMeshUploadData& out_mesh) const
    {
        return _impl->uploader
            ->upload_static_runtime_mesh(mesh_handle, mesh, *_impl->tracker, out_mesh);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_texture(
        const uint32 slot,
        const std::string& cache_key,
        const GraphicsTextureDesc& desc) const
    {
        return _impl->uploader->upload_texture(*_impl->tracker, slot, cache_key, desc);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_uniform_buffer(
        const uint32 slot,
        const std::string& debug_name,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        return _impl->uploader->upload_uniform_buffer(
            *_impl->tracker,
            slot,
            debug_name,
            cache_key,
            frame_index,
            data,
            byte_size);
    }
}
