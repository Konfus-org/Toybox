#include "systems/graphics/internal/resource_uploader_internal.h"
#include <utility>

namespace tbx::internal
{
    RenderingResourceUploader::RenderingResourceUploader(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _fallback_material(make_fallback_material())
        , _fallback_model(make_fallback_model())
        , _fallback_shader(make_fallback_shader())
    {
    }

    Result RenderingResourceUploader::upload_fallback_mesh(
        RenderingResourceTracker& resource_tracker,
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

    Result RenderingResourceUploader::upload_fallback_material(
        RenderingResourceTracker& resource_tracker,
        RenderingMaterialUploadData& out_material) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        if (!_fallback_material)
            return Result(false, "Resource uploader failed: fallback material unavailable.");

        const auto material_handle = Handle("Toybox/FallbackMaterial");
        const uint64 cache_key = make_material_upload_cache_key(material_handle, nullptr);
        if (const auto cached_material = _caches.materials.materials.find(cache_key);
            cached_material != _caches.materials.materials.end())
        {
            out_material = cached_material->second;
            track_material_upload_resources(resource_tracker, out_material);
            return {};
        }

        const Result result = upload_material_resources(
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
        if (result)
            _caches.materials.materials[cache_key] = out_material;

        return result;
    }

    void RenderingResourceUploader::cache_model_bounds(
        const Handle& model_handle,
        const MeshBounds& bounds) const
    {
        if (model_handle.is_valid() && bounds.is_valid)
            _caches.meshes.model_bounds[model_handle] = bounds;
    }

    GraphicsResourceBinding RenderingResourceUploader::upload_fallback_texture(
        RenderingResourceTracker& resource_tracker,
        const uint32 binding_id) const
    {
        const auto slot = resolve_shader_texture_slot(binding_id);
        const auto backend = _backend.lock();
        if (!backend || !slot.has_value())
            return GraphicsResourceBinding {.slot = slot.value_or(0U)};

        const auto binding = ::tbx::internal::upload_fallback_texture(
            *backend,
            resource_tracker,
            binding_id,
            _fallback_textures,
            _caches.textures);
        return binding.value_or(GraphicsResourceBinding {.slot = *slot});
    }

    Result RenderingResourceUploader::upload_material(
        const MaterialInstance& instance,
        RenderingResourceTracker& resource_tracker,
        RenderingMaterialUploadData& out_material) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Resource uploader failed: asset manager unavailable.");

        auto material_handle = resolve_material_handle(instance);
        const uint64 cache_key = make_material_upload_cache_key(material_handle, &instance);
        if (const auto cached_material = _caches.materials.materials.find(cache_key);
            cached_material != _caches.materials.materials.end())
        {
            out_material = cached_material->second;
            track_material_upload_resources(resource_tracker, out_material);
            return {};
        }

        auto loaded_material =
            asset_manager->load<Material>(material_handle, MaterialLoadParameters());
        if (!loaded_material)
            loaded_material = _fallback_material;
        if (!loaded_material)
            return Result(false, "Resource uploader failed: material load failed.");

        const auto material = *loaded_material;
        loaded_material.reset();

        const Result result = upload_material_resources(
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
        if (result)
            _caches.materials.materials[cache_key] = out_material;

        return result;
    }

    Result RenderingResourceUploader::upload_dynamic_mesh(
        const std::shared_ptr<DynamicMeshData>& mesh_data,
        RenderingResourceTracker& resource_tracker,
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

            if (try_update_mesh(*backend, resource_tracker, cached_mesh->second.mesh, mesh))
            {
                mesh_data->clear_dirty();
                out_mesh = cached_mesh->second.mesh;
                return {};
            }
        }

        const auto uploaded_mesh =
            upload_mesh(*backend, resource_tracker, Handle("Toybox/DynamicMesh"), mesh, 0U);
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

    Result RenderingResourceUploader::upload_bind_group(
        const BindGroupDesc& desc,
        RenderingResourceTracker& resource_tracker,
        Uuid& out_bind_group) const
    {
        out_bind_group = {};
        if (desc.bindings.empty())
            return {};

        const uint64 cache_hash = make_bind_group_cache_hash(desc);
        auto& entries = _caches.bind_groups.bind_groups[cache_hash];
        const auto cached = std::ranges::find_if(
            entries,
            [&desc](const RenderingBindGroupCacheEntry& entry)
            {
                return is_same_bind_group_key(entry.desc, desc);
            });
        if (cached != entries.end())
        {
            out_bind_group = cached->resource;
            track_bind_group_resources(resource_tracker, cached->desc);
            resource_tracker.track(out_bind_group);
            return {};
        }

        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const Result result = backend->create_bind_group(desc, out_bind_group);
        if (!result)
            return result;

        track_bind_group_resources(resource_tracker, desc);
        resource_tracker.track(out_bind_group);
        entries.push_back(
            RenderingBindGroupCacheEntry {
                .resource = out_bind_group,
                .desc = desc,
            });
        return {};
    }

    Result RenderingResourceUploader::upload_model_meshes(
        const Handle& model_handle,
        RenderingResourceTracker& resource_tracker,
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
            const auto mesh = upload_mesh(
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

    Result RenderingResourceUploader::upload_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto mesh_data = upload_mesh(*backend, resource_tracker, mesh_handle, mesh, 0U);
        if (!mesh_data.has_value())
            return Result(false, "Resource uploader failed: runtime mesh upload failed.");

        out_mesh = *mesh_data;
        return {};
    }

    bool RenderingResourceUploader::try_get_static_runtime_mesh(
        const Handle& mesh_handle,
        RenderingResourceTracker& resource_tracker,
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

    bool RenderingResourceUploader::try_get_model_bounds(
        const Handle& model_handle,
        MeshBounds& out_bounds) const
    {
        const auto bounds = _caches.meshes.model_bounds.find(model_handle);
        if (bounds == _caches.meshes.model_bounds.end())
            return false;

        out_bounds = bounds->second;
        return true;
    }

    Result RenderingResourceUploader::upload_static_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingResourceTracker& resource_tracker,
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

    GraphicsResourceBinding RenderingResourceUploader::upload_instance_buffer(
        RenderingResourceTracker& resource_tracker,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return GraphicsResourceBinding {.slot = VERTEX_BUFFER_SLOT_INSTANCE};

        return upload_instance_vertex_buffer(
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

    GraphicsResourceBinding RenderingResourceUploader::upload_uniform_buffer(
        RenderingResourceTracker& resource_tracker,
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

        return upload_or_update_uniform_buffer(
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

    GraphicsResourceBinding RenderingResourceUploader::upload_texture(
        RenderingResourceTracker& resource_tracker,
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
        const Result result = backend->create_texture(desc, resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering texture target upload failed: {}", result.get_report());
            return GraphicsResourceBinding {.slot = slot};
        }

        resource_tracker.track(resource);
        _caches.textures.render_targets[cache_key] = resource;
        return GraphicsResourceBinding {.slot = slot, .resource = resource};
    }

    void RenderingResourceUploader::discard_cached_resource(const Uuid& resource)
    {
        if (!resource.is_valid())
            return;

        discard_cached_bind_group_resource(_caches.bind_groups, resource);
        discard_cached_material_resource(_caches.materials, resource);
        erase_uuid_cache_entry(_caches.pipelines.pipelines, resource);
        discard_cached_mesh_resource(_caches.meshes, resource);
        erase_uuid_cache_entry(_caches.textures.textures, resource);
        erase_uuid_cache_entry(_caches.textures.default_textures, resource);
        erase_uuid_cache_entry(_caches.textures.render_targets, resource);
        discard_cached_uniform_resource(_caches.uniforms, resource);
        discard_cached_uniform_resource(_caches.instances, resource);
    }
}
