#include "tbx/systems/graphics/resource_manager.h"
#include "systems/assets/internal/fallbacks_internal.h"
#include "systems/graphics/internal/resource_manager_internal.h"
#include "systems/graphics/internal/resource_uploader_internal.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/model.h"
#include "tbx/types/material.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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

    internal::RenderingResourceUploader::RenderingResourceUploader(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _fallback_material(internal::make_fallback_material())
        , _fallback_model(internal::make_fallback_model())
        , _fallback_shader(internal::make_fallback_shader())
    {
    }

    Result internal::RenderingResourceUploader::upload_fallback_mesh(
        internal::RenderingResourceTracker& resource_tracker,
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        const auto fallback_mesh_handle = Handle("Toybox/FallbackMesh");
        if (const auto cached_mesh = _caches.meshes.runtime_meshes.find(fallback_mesh_handle);
            cached_mesh != _caches.meshes.runtime_meshes.end())
        {
            resource_tracker.track(cached_mesh->second.vertex_buffer);
            resource_tracker.track(cached_mesh->second.index_buffer);
            out_meshes.push_back(cached_mesh->second);
            return {};
        }

        auto mesh = RenderingMeshUploadData();
        const Result result =
            upload_static_runtime_mesh(fallback_mesh_handle, Mesh::CUBE, resource_tracker, mesh);
        if (!result)
            return Result(false, "Resource uploader failed: fallback mesh upload failed.");

        out_meshes.push_back(mesh);
        return {};
    }

    Result internal::RenderingResourceUploader::upload_fallback_material(
        internal::RenderingResourceTracker& resource_tracker,
        RenderingMaterialUploadData& out_material) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        if (!_fallback_material)
            return Result(false, "Resource uploader failed: fallback material unavailable.");

        const auto material_handle = Handle("Toybox/FallbackMaterial");
        return internal::upload_material_resources(
            *backend,
            nullptr,
            material_handle,
            *_fallback_material,
            nullptr,
            resource_tracker,
            _fallback_shader.get(),
            _fallback_textures,
            _caches,
            out_material);
    }

    GraphicsResourceBinding internal::RenderingResourceUploader::upload_fallback_texture(
        internal::RenderingResourceTracker& resource_tracker,
        const uint32 binding_id) const
    {
        const auto slot = resolve_shader_texture_slot(binding_id);
        const auto backend = _backend.lock();
        if (!backend || !slot.has_value())
            return GraphicsResourceBinding {.slot = slot.value_or(0U)};

        const auto binding = internal::upload_fallback_texture(
            *backend,
            resource_tracker,
            binding_id,
            _fallback_textures,
            _caches.textures);
        return binding.value_or(GraphicsResourceBinding {.slot = *slot});
    }

    Result internal::RenderingResourceUploader::upload_material(
        const MaterialInstance& instance,
        internal::RenderingResourceTracker& resource_tracker,
        RenderingMaterialUploadData& out_material) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Resource uploader failed: asset manager unavailable.");

        auto material_handle = internal::resolve_material_handle(instance);

        auto loaded_material =
            asset_manager->load<Material>(material_handle, MaterialLoadParameters());
        if (!loaded_material)
            loaded_material = _fallback_material;
        if (!loaded_material)
            return Result(false, "Resource uploader failed: material load failed.");

        const auto material = *loaded_material;
        loaded_material.reset();

        return internal::upload_material_resources(
            *backend,
            asset_manager.get(),
            material_handle,
            material,
            &instance,
            resource_tracker,
            _fallback_shader.get(),
            _fallback_textures,
            _caches,
            out_material);
    }

    Result internal::RenderingResourceUploader::upload_dynamic_mesh(
        const std::shared_ptr<DynamicMeshData>& mesh_data,
        internal::RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        if (!mesh_data)
            return Result(false, "Resource uploader failed: dynamic mesh data is unavailable.");

        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const Mesh& mesh = mesh_data->get_mesh();
        const DynamicMeshData* cache_key = mesh_data.get();
        auto cached_mesh = _caches.meshes.dynamic_meshes.find(cache_key);
        if (cached_mesh != _caches.meshes.dynamic_meshes.end()
            && cached_mesh->second.data.expired())
        {
            cached_mesh = _caches.meshes.dynamic_meshes.erase(cached_mesh);
        }

        if (cached_mesh != _caches.meshes.dynamic_meshes.end())
        {
            if (!mesh_data->is_dirty())
            {
                resource_tracker.track(cached_mesh->second.mesh.vertex_buffer);
                resource_tracker.track(cached_mesh->second.mesh.index_buffer);
                out_mesh = cached_mesh->second.mesh;
                return {};
            }

            if (internal::try_update_mesh(
                    *backend,
                    resource_tracker,
                    cached_mesh->second.mesh,
                    mesh))
            {
                mesh_data->clear_dirty();
                out_mesh = cached_mesh->second.mesh;
                return {};
            }
        }

        const auto uploaded_mesh = internal::upload_mesh(
            *backend,
            resource_tracker,
            Handle("Toybox/DynamicMesh"),
            mesh,
            0U);
        if (!uploaded_mesh.has_value())
            return Result(false, "Resource uploader failed: dynamic mesh upload failed.");

        _caches.meshes.dynamic_meshes[cache_key] = DynamicMeshResourceCacheEntry {
            .data = mesh_data,
            .mesh = *uploaded_mesh,
        };
        mesh_data->clear_dirty();
        out_mesh = *uploaded_mesh;
        return {};
    }

    Result internal::RenderingResourceUploader::upload_model_meshes(
        const Handle& model_handle,
        internal::RenderingResourceTracker& resource_tracker,
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        if (const auto cached_meshes = _caches.meshes.model_meshes.find(model_handle);
            cached_meshes != _caches.meshes.model_meshes.end())
        {
            for (const auto& mesh : cached_meshes->second)
            {
                resource_tracker.track(mesh.vertex_buffer);
                resource_tracker.track(mesh.index_buffer);
                out_meshes.push_back(mesh);
            }
            return {};
        }

        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Resource uploader failed: asset manager unavailable.");

        auto model = asset_manager->load<Model>(model_handle, ModelLoadParameters());
        if (!model)
            model = _fallback_model;
        if (!model)
            return Result(false, "Resource uploader failed: model load failed.");

        for (uint mesh_index = 0U; mesh_index < static_cast<uint>(model->meshes.size());
             ++mesh_index)
        {
            const auto mesh = internal::upload_mesh(
                *backend,
                resource_tracker,
                model_handle,
                model->meshes[static_cast<size>(mesh_index)],
                mesh_index);
            if (mesh.has_value())
            {
                out_meshes.push_back(*mesh);
            }
        }

        if (!out_meshes.empty())
            _caches.meshes.model_meshes[model_handle] = out_meshes;

        return {};
    }

    Result internal::RenderingResourceUploader::upload_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        internal::RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto mesh_data =
            internal::upload_mesh(*backend, resource_tracker, mesh_handle, mesh, 0U);
        if (!mesh_data.has_value())
            return Result(false, "Resource uploader failed: runtime mesh upload failed.");

        out_mesh = *mesh_data;
        return {};
    }

    bool internal::RenderingResourceUploader::try_get_static_runtime_mesh(
        const Handle& mesh_handle,
        internal::RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        const auto cached_mesh = _caches.meshes.runtime_meshes.find(mesh_handle);
        if (cached_mesh == _caches.meshes.runtime_meshes.end())
            return false;

        resource_tracker.track(cached_mesh->second.vertex_buffer);
        resource_tracker.track(cached_mesh->second.index_buffer);
        out_mesh = cached_mesh->second;
        return true;
    }

    Result internal::RenderingResourceUploader::upload_static_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        internal::RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        if (try_get_static_runtime_mesh(mesh_handle, resource_tracker, out_mesh))
            return {};

        const Result result = upload_runtime_mesh(mesh_handle, mesh, resource_tracker, out_mesh);
        if (!result)
            return result;

        _caches.meshes.runtime_meshes[mesh_handle] = out_mesh;
        return {};
    }

    GraphicsResourceBinding internal::RenderingResourceUploader::upload_instance_buffer(
        internal::RenderingResourceTracker& resource_tracker,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        (void)frame_index;
        const auto backend = _backend.lock();
        if (!backend)
            return GraphicsResourceBinding {.slot = VERTEX_BUFFER_SLOT_INSTANCE};

        return internal::upload_instance_vertex_buffer(
            *backend,
            resource_tracker,
            _caches.instances,
            VERTEX_BUFFER_SLOT_INSTANCE,
            std::string("Instance Shader Data ") + cache_key,
            cache_key,
            frame_index,
            data,
            byte_size);
    }

    GraphicsResourceBinding internal::RenderingResourceUploader::upload_uniform_buffer(
        internal::RenderingResourceTracker& resource_tracker,
        const uint32 slot,
        const std::string& debug_name,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return GraphicsResourceBinding {.slot = slot};

        return internal::upload_or_update_uniform_buffer(
            *backend,
            resource_tracker,
            _caches.uniforms,
            slot,
            debug_name,
            cache_key,
            frame_index,
            data,
            byte_size);
    }

    GraphicsResourceBinding internal::RenderingResourceUploader::upload_texture(
        internal::RenderingResourceTracker& resource_tracker,
        const uint32 slot,
        const std::string& cache_key,
        const GraphicsTextureDesc& desc) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return GraphicsResourceBinding {.slot = slot};

        if (const auto cached = _caches.textures.render_targets.find(cache_key);
            cached != _caches.textures.render_targets.end())
        {
            resource_tracker.track(cached->second);
            return GraphicsResourceBinding {.slot = slot, .resource = cached->second};
        }

        auto resource = Uuid {};
        const Result result = backend->upload_texture(desc, nullptr, 0U, resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering texture target upload failed: {}", result.get_report());
            return GraphicsResourceBinding {.slot = slot};
        }

        resource_tracker.track(resource);
        _caches.textures.render_targets[cache_key] = resource;
        return GraphicsResourceBinding {.slot = slot, .resource = resource};
    }

    void internal::RenderingResourceUploader::discard_cached_resource(const Uuid& resource)
    {
        if (!resource.is_valid())
            return;

        internal::erase_uuid_cache_entry(_caches.pipelines.pipelines, resource);
        internal::discard_cached_mesh_resource(_caches.meshes, resource);
        internal::erase_uuid_cache_entry(_caches.textures.textures, resource);
        internal::erase_uuid_cache_entry(_caches.textures.default_textures, resource);
        internal::erase_uuid_cache_entry(_caches.textures.render_targets, resource);
        internal::discard_cached_uniform_resource(_caches.uniforms, resource);
        internal::discard_cached_uniform_resource(_caches.instances, resource);
    }
}
