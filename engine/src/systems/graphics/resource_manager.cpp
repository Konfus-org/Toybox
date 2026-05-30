#include "tbx/systems/graphics/resource_manager.h"
#include "rendering_resource_tracker.h"
#include "rendering_resource_uploader.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/resource_upload_caches.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/vertex.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <memory>
#include <vector>


namespace tbx
{
    //// RESOURCE CACHES ////

    void MeshResourceCache::cache_model_bounds(const Handle& model_handle, const MeshBounds& bounds)
    {
        if (has_asset_reference(model_handle) && bounds.is_valid)
            model_bounds[model_handle] = bounds;
    }

    bool MeshResourceCache::try_get_model_bounds(const Handle& model_handle, MeshBounds& out_bounds)
        const
    {
        const auto bounds = model_bounds.find(model_handle);
        if (bounds == model_bounds.end())
            return false;

        out_bounds = bounds->second;
        return true;
    }

    //// RESOURCE MANAGER ////

    struct RenderingResourceManager::State
    {
        State(
            std::weak_ptr<IGraphicsBackend> graphics_backend,
            std::weak_ptr<AssetManager> asset_manager,
            const float unload_time_seconds)
            : backend(graphics_backend)
            , tracker(std::make_unique<RenderingResourceTracker>())
            , uploader(
                  std::make_unique<RenderingResourceUploader>(
                      std::move(graphics_backend),
                      std::move(asset_manager)))
            , resource_unload_time_seconds(std::max(0.0F, unload_time_seconds))
        {
        }

        std::weak_ptr<IGraphicsBackend> backend = {};
        std::unique_ptr<RenderingResourceTracker> tracker = {};
        std::unique_ptr<RenderingResourceUploader> uploader = {};
        float resource_unload_time_seconds = 3.0F;
    };

    RenderingResourceManager::RenderingResourceManager(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        const float resource_unload_time_seconds)
        : _state(
              std::make_unique<State>(
                  std::move(backend),
                  std::move(asset_manager),
                  resource_unload_time_seconds))
    {
    }

    RenderingResourceManager::~RenderingResourceManager() = default;

    void RenderingResourceManager::cache_model_bounds(
        const Handle& model_handle,
        const MeshBounds& bounds) const
    {
        _state->uploader->cache_model_bounds(model_handle, bounds);
    }

    float RenderingResourceManager::get_resource_unload_time_seconds() const
    {
        return _state->resource_unload_time_seconds;
    }

    bool RenderingResourceManager::is_managed(const Uuid& resource) const
    {
        return resource.is_valid() && _state->tracker->is_tracked(resource);
    }

    void RenderingResourceManager::set_resource_unload_time_seconds(const float seconds)
    {
        _state->resource_unload_time_seconds = std::max(0.0F, seconds);
    }

    bool RenderingResourceManager::try_get_model_bounds(
        const Handle& model_handle,
        MeshBounds& out_bounds) const
    {
        return _state->uploader->try_get_model_bounds(model_handle, out_bounds);
    }

    void RenderingResourceManager::update(const DeltaTime delta)
    {
        _state->tracker->update(delta);

        const auto backend = _state->backend.lock();
        if (!backend)
            return;

        auto stale_resources = std::vector<uint>();
        const auto& tracked_resources = _state->tracker->get_tracked_resources();
        for (const uint resource : tracked_resources)
        {
            if (_state->tracker->get_time_alive(resource) < _state->resource_unload_time_seconds)
                continue;

            stale_resources.push_back(resource);
        }

        for (const uint resource : stale_resources)
        {
            const auto resource_id = Uuid(resource);
            if (backend->destroy_resource(resource_id))
            {
                _state->uploader->discard_cached_resource(resource_id);
                _state->tracker->untrack(resource);
            }
        }
    }

    Uuid RenderingResourceManager::upload_bind_group(const BindGroupDesc& desc) const
    {
        return _state->uploader->upload_bind_group(desc, *_state->tracker);
    }

    RenderingMeshUploadData RenderingResourceManager::upload_dynamic_mesh(
        const std::shared_ptr<DynamicMeshData>& mesh_data) const
    {
        return _state->uploader->upload_dynamic_mesh(mesh_data, *_state->tracker);
    }

    RenderingMeshUploadData RenderingResourceManager::upload_dynamic_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh) const
    {
        return _state->uploader->upload_runtime_mesh(mesh_handle, mesh, *_state->tracker);
    }

    RenderingMaterialUploadData RenderingResourceManager::upload_fallback_material() const
    {
        return _state->uploader->upload_fallback_material(*_state->tracker);
    }

    std::vector<RenderingMeshUploadData> RenderingResourceManager::upload_fallback_mesh() const
    {
        return _state->uploader->upload_fallback_mesh(*_state->tracker);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_fallback_texture(
        const uint32 binding_id) const
    {
        return _state->uploader->upload_fallback_texture(*_state->tracker, binding_id);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_instance_buffer(
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        return _state->uploader
            ->upload_instance_buffer(*_state->tracker, cache_key, frame_index, data, byte_size);
    }

    RenderingMaterialUploadData RenderingResourceManager::upload_material(
        const MaterialInstance& instance) const
    {
        return _state->uploader->upload_material(instance, *_state->tracker);
    }

    std::vector<RenderingMeshUploadData> RenderingResourceManager::upload_model(
        const Handle& model_handle) const
    {
        return _state->uploader->upload_model_meshes(model_handle, *_state->tracker);
    }

    RenderingMeshUploadData RenderingResourceManager::upload_static_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh) const
    {
        return _state->uploader->upload_static_runtime_mesh(mesh_handle, mesh, *_state->tracker);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_texture(
        const uint32 slot,
        const std::string& cache_key,
        const GraphicsTextureDesc& desc) const
    {
        return _state->uploader->upload_texture(*_state->tracker, slot, cache_key, desc);
    }

    GraphicsResourceBinding RenderingResourceManager::upload_uniform_buffer(
        const uint32 slot,
        const std::string& debug_name,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        return _state->uploader->upload_uniform_buffer(
            *_state->tracker,
            slot,
            debug_name,
            cache_key,
            frame_index,
            data,
            byte_size);
    }

}
