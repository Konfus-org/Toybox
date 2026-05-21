#include "tbx/systems/graphics/resource_uploader.h"
#include "systems/graphics/internal/resource_uploader_internal.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/model.h"
#include "tbx/types/material.h"
#include "tbx/types/texture.h"
#include "tbx/types/vertex.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace tbx
{
    ResourceUploader::ResourceUploader(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
    {
    }

    Result ResourceUploader::upload_fallback_mesh(
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

    Result ResourceUploader::upload_material(
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

        auto material_handle = internal::resolve_material_handle(instance);

        auto loaded_material =
            asset_manager->load<Material>(material_handle, MaterialLoadParameters());
        if (!loaded_material)
            loaded_material = make_fallback_material();
        if (!loaded_material)
            return Result(false, "Resource uploader failed: material load failed.");

        const auto material = *loaded_material;
        loaded_material.reset();

        auto parameters = material.parameters;
        auto textures = material.textures;
        const auto config = internal::resolve_material_config(material, instance);

        if (instance.overrides.has_parameter_override)
            for (const auto& parameter : instance.overrides.parameters)
                parameters.set(parameter);
        if (instance.overrides.has_texture_override)
            for (const auto& texture : instance.overrides.textures)
                textures.set(texture);

        auto pipeline = Uuid {};
        const std::string pipeline_cache_key =
            internal::make_material_pipeline_cache_key(material_handle, config);
        if (const auto cached_pipeline = _caches.pipelines.pipelines.find(pipeline_cache_key);
            cached_pipeline != _caches.pipelines.pipelines.end())
        {
            pipeline = cached_pipeline->second;
            resource_tracker.track(pipeline);
        }
        else
        {
            const ShaderProgram shader =
                internal::build_material_shader(*asset_manager, material_handle, material);
            const GraphicsPipelineDesc pipeline_desc =
                internal::make_material_pipeline_desc(material_handle, shader, config);

            const Result pipeline_result = backend->upload_pipeline(pipeline_desc, pipeline);
            if (!pipeline_result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Rendering pipeline upload failed: {}",
                    pipeline_result.get_report());
                return Result(false, "Resource uploader failed: material pipeline upload failed.");
            }
            resource_tracker.track(pipeline);
            _caches.pipelines.pipelines[pipeline_cache_key] = pipeline;
        }

        out_material = RenderingMaterialUploadData {
            .pipeline = pipeline,
            .uniform_values = internal::make_material_uniform_values(parameters),
        };
        out_material.textures.reserve(textures.values.size());
        for (const auto& texture : textures)
        {
            const auto texture_binding = internal::upload_material_texture(
                *backend,
                *asset_manager,
                resource_tracker,
                texture,
                _caches.textures);
            if (texture_binding.has_value())
                out_material.textures.push_back(*texture_binding);
        }

        return {};
    }

    MaterialConfig ResourceUploader::get_material_config(const MaterialInstance& instance) const
    {
        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return instance.has_config_override_enabled() ? instance.overrides.config
                                                          : MaterialConfig {};

        const auto material_handle = internal::resolve_material_handle(instance);
        auto loaded_material =
            asset_manager->load<Material>(material_handle, MaterialLoadParameters());
        if (!loaded_material)
            loaded_material = make_fallback_material();
        if (!loaded_material)
            return instance.has_config_override_enabled() ? instance.overrides.config
                                                          : MaterialConfig {};

        return internal::resolve_material_config(*loaded_material, instance);
    }

    Result ResourceUploader::upload_dynamic_mesh(
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

            if (internal::try_update_mesh(
                    *backend,
                    resource_tracker,
                    cached_mesh->second.mesh,
                    mesh))
            {
#if defined(TBX_ENABLE_VERBOSE)
                if (internal::active_render_metrics)
                    ++internal::active_render_metrics->dynamic_mesh_update_count;
#endif
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
#if defined(TBX_ENABLE_VERBOSE)
        if (internal::active_render_metrics)
            ++internal::active_render_metrics->dynamic_mesh_upload_count;
#endif
        mesh_data->clear_dirty();
        out_mesh = *uploaded_mesh;
        return {};
    }

    Result ResourceUploader::upload_model_meshes(
        const Handle& model_handle,
        RenderingResourceTracker& resource_tracker,
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        if (const auto cached_meshes = _caches.meshes.model_meshes.find(model_handle);
            cached_meshes != _caches.meshes.model_meshes.end())
        {
#if defined(TBX_ENABLE_VERBOSE)
            if (internal::active_render_metrics)
                internal::active_render_metrics->model_mesh_cache_hit_count +=
                    static_cast<uint64>(cached_meshes->second.size());
#endif
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
            model = make_fallback_model();
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
#if defined(TBX_ENABLE_VERBOSE)
                if (internal::active_render_metrics)
                    ++internal::active_render_metrics->model_mesh_upload_count;
#endif
                out_meshes.push_back(*mesh);
            }
        }

        if (!out_meshes.empty())
            _caches.meshes.model_meshes[model_handle] = out_meshes;

        return {};
    }

    Result ResourceUploader::upload_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingResourceTracker& resource_tracker,
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

    bool ResourceUploader::try_get_static_runtime_mesh(
        const Handle& mesh_handle,
        RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        const auto cached_mesh = _caches.meshes.runtime_meshes.find(mesh_handle);
        if (cached_mesh == _caches.meshes.runtime_meshes.end())
            return false;

#if defined(TBX_ENABLE_VERBOSE)
        if (internal::active_render_metrics)
            ++internal::active_render_metrics->static_mesh_cache_hit_count;
#endif
        resource_tracker.track(cached_mesh->second.vertex_buffer);
        resource_tracker.track(cached_mesh->second.index_buffer);
        out_mesh = cached_mesh->second;
        return true;
    }

    bool ResourceUploader::has_static_runtime_mesh(const Handle& mesh_handle) const
    {
        return _caches.meshes.runtime_meshes.contains(mesh_handle);
    }

    Result ResourceUploader::upload_static_runtime_mesh(
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

#if defined(TBX_ENABLE_VERBOSE)
        if (internal::active_render_metrics)
            ++internal::active_render_metrics->static_mesh_upload_count;
#endif
        _caches.meshes.runtime_meshes[mesh_handle] = out_mesh;
        return {};
    }

    GraphicsResourceBinding ResourceUploader::upload_instance_buffer(
        RenderingResourceTracker& resource_tracker,
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

    GraphicsResourceBinding ResourceUploader::upload_uniform_buffer(
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

    GraphicsResourceBinding ResourceUploader::upload_texture(
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
        const Result result = backend->upload_texture(desc, nullptr, 0U, resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering texture target upload failed: {}", result.get_report());
            return GraphicsResourceBinding {.slot = slot};
        }

        resource_tracker.track(resource);
        _caches.textures.render_targets[cache_key] = resource;
#if defined(TBX_ENABLE_VERBOSE)
        if (internal::active_render_metrics)
            ++internal::active_render_metrics->texture_target_upload_count;
#endif
        return GraphicsResourceBinding {.slot = slot, .resource = resource};
    }

    void ResourceUploader::discard_cached_resource(const Uuid& resource)
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
