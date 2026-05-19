#include "tbx/systems/graphics/resource_manager.h"
#include "graphics_resource_factory.h"
#include "resources/graphics_resource_requests.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/utils/hash.h"
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    // KEEP IN SYNC WITH ShaderBase.glsl in resources
    static constexpr uint32 MATERIAL_RESOURCE_BUCKET = 1U;
    static constexpr uint32 POST_PROCESS_MATERIAL_RESOURCE_BUCKET = 2U;
    static constexpr uint32 MODEL_RESOURCE_BUCKET = 3U;
    static constexpr uint32 TEXTURE_RESOURCE_BUCKET = 4U;
    static constexpr uint32 RUNTIME_RESOURCE_BUCKET = 5U;
    static constexpr uint32 TBX_BINDING_ALBEDO_MAP = 10U;
    static constexpr uint32 TBX_BINDING_NORMAL_MAP = 11U;
    static constexpr uint32 TBX_BINDING_METALLIC_ROUGHNESS_MAP = 12U;
    static constexpr uint32 TBX_BINDING_AO_MAP = 13U;
    static constexpr uint32 TBX_BINDING_EMISSIVE_MAP = 14U;
    static constexpr uint32 TBX_BINDING_SHADOW_MASK = 31U;
    static constexpr uint32 TBX_BINDING_GBUFFER_ALBEDO = 50U;
    static constexpr uint32 TBX_BINDING_GBUFFER_NORMAL = 51U;
    static constexpr uint32 TBX_BINDING_GBUFFER_MATERIAL = 52U;
    static constexpr uint32 TBX_BINDING_GBUFFER_EMISSIVE = 53U;
    static constexpr uint32 TBX_BINDING_GBUFFER_DEPTH = 54U;
    static constexpr uint32 TBX_BINDING_POST_SOURCE_COLOR = 60U;
    static constexpr uint32 TBX_BINDING_POST_SOURCE_DEPTH = 61U;

    static std::optional<uint32> resolve_shader_texture_slot(const std::string_view binding_name)
    {
        if (binding_name == "u_albedo_map")
        {
            return TBX_BINDING_ALBEDO_MAP;
        }

        if (binding_name == "u_normal_map")
            return TBX_BINDING_NORMAL_MAP;

        if (binding_name == "u_metallic_roughness_map")
        {
            return TBX_BINDING_METALLIC_ROUGHNESS_MAP;
        }

        if (binding_name == "u_ao_map")
            return TBX_BINDING_AO_MAP;

        if (binding_name == "u_emissive_map")
            return TBX_BINDING_EMISSIVE_MAP;

        if (binding_name == "u_gbuffer_albedo")
        {
            return TBX_BINDING_GBUFFER_ALBEDO;
        }

        if (binding_name == "u_gbuffer_normal")
        {
            return TBX_BINDING_GBUFFER_NORMAL;
        }

        if (binding_name == "u_gbuffer_material")
        {
            return TBX_BINDING_GBUFFER_MATERIAL;
        }

        if (binding_name == "u_gbuffer_emissive")
        {
            return TBX_BINDING_GBUFFER_EMISSIVE;
        }

        if (binding_name == "u_gbuffer_depth")
        {
            return TBX_BINDING_GBUFFER_DEPTH;
        }

        if (binding_name == "u_shadow_mask")
        {
            return TBX_BINDING_SHADOW_MASK;
        }

        if (binding_name == "u_source_color")
            return TBX_BINDING_POST_SOURCE_COLOR;

        if (binding_name == "u_source_depth")
            return TBX_BINDING_POST_SOURCE_DEPTH;

        if (binding_name == "u_lut")
            return 1U;

        return std::nullopt;
    }

    GraphicsResourceManager::GraphicsResourceManager(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        const uint unused_frame_limit)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _backend_ref(_backend.lock())
        , _asset_manager_ref(_asset_manager.lock())
        , _unused_frame_limit(unused_frame_limit)
    {
    }

    GraphicsResourceManager::~GraphicsResourceManager() noexcept
    {
        unload_all();
    }

    void GraphicsResourceManager::set_unused_frame_limit(const uint unused_frame_limit)
    {
        _unused_frame_limit = unused_frame_limit;
    }

    std::optional<GraphicsResourceUsage> GraphicsResourceManager::get_usage(const Handle& handle)
    {
        return find_usage(handle);
    }

    bool GraphicsResourceManager::is_loaded(const Handle& handle)
    {
        return find_usage(handle).has_value();
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return upload_cached_material(handle, parameters, out_resource_uuid);
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = upload(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    bool GraphicsResourceManager::upload(
        const MaterialInstance& instance,
        GraphicsMaterialInstanceResource& out_material_resource)
    {
        out_material_resource = {};

        const Handle& handle = instance.get_handle();
        if (!handle.is_valid())
            return load_fallback_material_resource(out_material_resource);

        const Uuid asset_id = resolve_asset_id(handle);
        if (_failed_materials.contains(asset_id))
            return load_fallback_material_resource(out_material_resource);

        auto pipeline_resource = Uuid {};
        if (const auto result = upload(handle, MaterialLoadParameters {}, pipeline_resource);
            !result)
        {
            _failed_materials.insert(asset_id);
            return load_fallback_material_resource(out_material_resource);
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: asset manager is unavailable.");
            return false;
        }

        const std::shared_ptr<Material> material =
            asset_manager->load<Material>(handle, MaterialLoadParameters());
        if (!material)
        {
            _failed_materials.insert(asset_id);
            return load_fallback_material_resource(out_material_resource);
        }

        out_material_resource = GraphicsMaterialInstanceResource {
            .material = handle,
            .pipeline = pipeline_resource,
            .parameters = material->parameters,
            .textures = material->textures,
            .config = instance.has_config_override_enabled() ? instance.config : material->config,
        };

        for (const auto& parameter : instance.param_overrides)
            out_material_resource.parameters.set(parameter);

        for (const auto& texture : instance.texture_overrides)
            out_material_resource.textures.set(texture);

        return true;
    }

    bool GraphicsResourceManager::upload(
        const MaterialInstance& instance,
        GraphicsMaterialDrawResource& out_material_resource,
        const GraphicsMaterialUploadMode mode)
    {
        out_material_resource = {};

        auto material_resource = GraphicsMaterialInstanceResource {};
        if (mode == GraphicsMaterialUploadMode::STANDARD)
        {
            if (const auto result = upload(instance, material_resource); !result)
                return result;
        }
        else
        {
            const Handle& handle = instance.get_handle();
            if (!handle.is_valid())
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics resource manager: post-process material handle is invalid.");
                return false;
            }

            const Uuid asset_id = resolve_asset_id(handle);
            if (!asset_id.is_valid())
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics resource manager: post-process material asset id is invalid.");
                return false;
            }

            const auto asset_manager = lock_asset_manager();
            if (!asset_manager)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics resource manager: asset manager is unavailable.");
                return false;
            }

            const std::shared_ptr<Material> material =
                asset_manager->load<Material>(handle, MaterialLoadParameters());
            if (!material)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics resource manager: failed to load post-process material asset.");
                return false;
            }

            const GraphicsResourceKey key =
                make_asset_resource_key(asset_id, POST_PROCESS_MATERIAL_RESOURCE_BUCKET);
            auto pipeline = Uuid {};
            if (auto* record = find_record(key); record != nullptr)
            {
                touch_resource(key);
                pipeline = record->usage.resource;
            }
            else
            {
                auto resource = std::shared_ptr<GraphicsResource> {};
                if (const auto result =
                        upload_post_process_material_resource(handle, *material, resource, pipeline);
                    !result)
                {
                    return result;
                }

                track_resource(key, handle, pipeline, std::move(resource));
            }

            material_resource = GraphicsMaterialInstanceResource {
                .material = handle,
                .pipeline = pipeline,
                .parameters = material->parameters,
                .textures = material->textures,
                .config =
                    instance.has_config_override_enabled() ? instance.config : material->config,
            };

            for (const auto& parameter : instance.param_overrides)
                material_resource.parameters.set(parameter);
            for (const auto& texture : instance.texture_overrides)
                material_resource.textures.set(texture);
        }

        out_material_resource.uniform_data =
            make_material_uniform_data(material_resource.parameters);
        out_material_resource.parameter_names.reserve(material_resource.parameters.values.size());
        for (const auto& parameter : material_resource.parameters)
            out_material_resource.parameter_names.push_back(parameter.name);

        out_material_resource.texture_names.reserve(material_resource.textures.values.size());
        for (const auto& texture_binding : material_resource.textures)
            out_material_resource.texture_names.push_back(texture_binding.name);

        if (const auto result =
                load_material_textures(material_resource.textures, out_material_resource.textures);
            !result)
        {
            return result;
        }

        out_material_resource.pipeline = material_resource.pipeline;
        out_material_resource.uniform_key = make_material_key(
            out_material_resource.pipeline,
            out_material_resource.uniform_data,
            out_material_resource.textures);
        return true;
    }

    uint GraphicsResourceManager::unload_stale()
    {
        _current_frame += 1U;
        return unload_unused();
    }

    bool GraphicsResourceManager::load_default_texture(Uuid& out_resource_uuid)
    {
        if (_default_texture.is_valid())
        {
            out_resource_uuid = _default_texture;
            return true;
        }

        const auto texture = Texture(
            Size {1U, 1U},
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            TextureFormat::RGBA,
            TextureMipmaps::DISABLED,
            TextureCompression::DISABLED,
            std::vector<Pixel> {255U, 255U, 255U, 255U});

        if (const auto result = upload_texture_resource(
                Handle("Toybox/DefaultWhiteTexture"),
                texture,
                _default_texture_resource,
                _default_texture);
            !result)
        {
            return result;
        }

        out_resource_uuid = _default_texture;
        return true;
    }

    bool GraphicsResourceManager::ensure_solid_fallback_texture(
        const std::string_view debug_name,
        const Pixel r,
        const Pixel g,
        const Pixel b,
        const Pixel a,
        std::shared_ptr<GraphicsResource>& out_resource,
        Uuid& out_resource_uuid)
    {
        if (out_resource_uuid.is_valid())
            return true;

        const auto texture = Texture(
            Size {1U, 1U},
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            TextureFormat::RGBA,
            TextureMipmaps::DISABLED,
            TextureCompression::DISABLED,
            std::vector<Pixel> {r, g, b, a});

        return upload_texture_resource(
            Handle(std::string(debug_name)),
            texture,
            out_resource,
            out_resource_uuid);
    }

    bool GraphicsResourceManager::load_default_texture_for_binding(
        const std::string_view binding_name,
        Uuid& out_resource_uuid)
    {
        if (binding_name == "u_normal_map")
        {
            if (const auto result = ensure_solid_fallback_texture(
                    "Toybox/DefaultNormalTexture",
                    static_cast<Pixel>(128U),
                    static_cast<Pixel>(128U),
                    static_cast<Pixel>(255U),
                    static_cast<Pixel>(255U),
                    _default_normal_texture_resource,
                    _default_normal_texture);
                !result)
            {
                return result;
            }

            out_resource_uuid = _default_normal_texture;
            return true;
        }

        if (binding_name == "u_emissive_map" || binding_name == "u_shadow_mask"
            || binding_name == "u_source_depth")
        {
            if (const auto result = ensure_solid_fallback_texture(
                    "Toybox/DefaultBlackTexture",
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(255U),
                    _default_black_texture_resource,
                    _default_black_texture);
                !result)
            {
                return result;
            }

            out_resource_uuid = _default_black_texture;
            return true;
        }

        if (binding_name == "u_metallic_roughness_map")
        {
            if (const auto result = ensure_solid_fallback_texture(
                    "Toybox/DefaultMetallicRoughnessTexture",
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(255U),
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(255U),
                    _default_metallic_roughness_texture_resource,
                    _default_metallic_roughness_texture);
                !result)
            {
                return result;
            }

            out_resource_uuid = _default_metallic_roughness_texture;
            return true;
        }

        return load_default_texture(out_resource_uuid);
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return upload_cached_model(handle, parameters, out_resource_uuid);
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = upload(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        GraphicsModelResource& out_model_resource)
    {
        out_model_resource = {};
        auto resource_uuid = Uuid {};
        if (const auto result = upload(handle, parameters, resource_uuid); !result)
            return result;

        const Uuid asset_id = resolve_asset_id(handle);
        const GraphicsResourceKey key = make_asset_resource_key(asset_id, MODEL_RESOURCE_BUCKET);
        const auto* record = find_record(key);
        if (record == nullptr)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: model resource metadata was missing.");
            return false;
        }

        const auto* meshes =
            std::any_cast<std::vector<GraphicsModelMeshResource>>(&record->payload);
        if (meshes == nullptr)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: model resource metadata was invalid.");
            return false;
        }

        out_model_resource = GraphicsModelResource {
            .asset = handle,
            .resource = record->usage.resource,
            .meshes = *meshes,
        };
        return true;
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return upload_cached_texture(handle, parameters, out_resource_uuid);
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = upload(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    bool GraphicsResourceManager::upload(
        const Handle& handle,
        const Mesh& mesh,
        GraphicsModelResource& out_model_resource)
    {
        out_model_resource = {};
        if (!handle.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: mesh handle is invalid.");
            return false;
        }

        const Uuid asset_id = resolve_asset_id(handle);
        const GraphicsResourceKey key = make_asset_resource_key(asset_id, MODEL_RESOURCE_BUCKET);
        if (auto* record = find_record(key); record != nullptr)
        {
            touch_resource(key);
            const auto* meshes =
                std::any_cast<std::vector<GraphicsModelMeshResource>>(&record->payload);
            if (meshes == nullptr)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics resource manager: mesh resource metadata invalid.");
                return false;
            }

            out_model_resource = GraphicsModelResource {
                .asset = handle,
                .resource = record->usage.resource,
                .meshes = *meshes,
            };
            return true;
        }

        auto model = Model {};
        model.meshes.push_back(mesh);

        auto resource_uuid = Uuid {};
        auto resource = std::shared_ptr<GraphicsResource> {};
        auto mesh_resources = std::vector<GraphicsModelMeshResource> {};
        if (const auto result = upload_model_resource(
                handle,
                model,
                resource,
                mesh_resources,
                resource_uuid);
            !result)
        {
            return result;
        }

        track_resource(key, handle, resource_uuid, std::move(resource), mesh_resources);
        out_model_resource = GraphicsModelResource {
            .asset = handle,
            .resource = resource_uuid,
            .meshes = std::move(mesh_resources),
        };
        return true;
    }

    bool GraphicsResourceManager::upload(
        const GraphicsBufferDesc& desc,
        const void* data,
        const uint64 data_size,
        Uuid& out_resource_uuid)
    {
        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        auto resource = std::shared_ptr<GraphicsResource> {};
        if (const auto result =
                resource_factory.create_buffer_resource(desc, data, data_size, resource);
            !result)
        {
            return result;
        }

        return upload_runtime_resource(Handle(desc.debug_name), std::move(resource), out_resource_uuid, {});
    }

    bool GraphicsResourceManager::upload(
        const GraphicsPipelineDesc& desc,
        Uuid& out_resource_uuid)
    {
        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        auto resource = std::shared_ptr<GraphicsResource> {};
        if (const auto result = resource_factory.create_pipeline_resource(desc, resource); !result)
            return result;

        return upload_runtime_resource(Handle(desc.debug_name), std::move(resource), out_resource_uuid, {});
    }

    bool GraphicsResourceManager::upload(const GraphicsSamplerDesc& desc, Uuid& out_resource_uuid)
    {
        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        auto resource = std::shared_ptr<GraphicsResource> {};
        if (const auto result = resource_factory.create_sampler_resource(desc, resource); !result)
            return result;

        return upload_runtime_resource(Handle(desc.debug_name), std::move(resource), out_resource_uuid, {});
    }

    bool GraphicsResourceManager::upload(
        const GraphicsTextureDesc& desc,
        const void* data,
        const uint64 data_size,
        Uuid& out_resource_uuid)
    {
        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        auto resource = std::shared_ptr<GraphicsResource> {};
        if (const auto result =
                resource_factory.create_texture_resource(desc, data, data_size, resource);
            !result)
        {
            return result;
        }

        return upload_runtime_resource(Handle(desc.debug_name), std::move(resource), out_resource_uuid, {});
    }

    bool GraphicsResourceManager::update(const Uuid& resource_uuid)
    {
        const bool tracked = touch_runtime_resource(resource_uuid);
        if (!tracked)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: runtime resource is not tracked.");
        }
        return tracked;
    }

    bool GraphicsResourceManager::update(
        const Uuid& resource_uuid,
        const void* data,
        const uint64 data_size,
        const uint64 offset)
    {
        touch_runtime_resource(resource_uuid);
        const GraphicsResourceKey key = make_runtime_resource_key(resource_uuid);
        auto* record = find_record(key);
        if (record == nullptr || !record->resource)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: runtime resource is not tracked.");
            return false;
        }

        record->resource->update(
            GraphicsBufferUpdateRequest {
                .data = data,
                .data_size = data_size,
                .offset = offset,
            });
        return true;
    }

    bool GraphicsResourceManager::update(
        const Uuid& resource_uuid,
        const GraphicsTextureUpdateDesc& desc,
        const void* data,
        const uint64 data_size)
    {
        touch_runtime_resource(resource_uuid);
        const GraphicsResourceKey key = make_runtime_resource_key(resource_uuid);
        auto* record = find_record(key);
        if (record == nullptr || !record->resource)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: runtime resource is not tracked.");
            return false;
        }

        record->resource->update(
            GraphicsTextureUpdateRequest {
                .desc = desc,
                .data = data,
                .data_size = data_size,
            });
        return true;
    }

    bool GraphicsResourceManager::unload(const Handle& handle)
    {
        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: asset id is invalid.");
            return false;
        }

        const auto buckets = {
            MATERIAL_RESOURCE_BUCKET,
            POST_PROCESS_MATERIAL_RESOURCE_BUCKET,
            MODEL_RESOURCE_BUCKET,
            TEXTURE_RESOURCE_BUCKET,
        };
        auto unloaded_count = 0U;
        auto found = false;
        for (const uint32 bucket : buckets)
        {
            const GraphicsResourceKey key = make_asset_resource_key(asset_id, bucket);
            const GraphicsResourceRecord* record = find_record(key);
            if (record == nullptr)
                continue;

            if (const auto result = unload_resource(key, *record); result)
            {
                erase_usage(key);
                unloaded_count += 1U;
                found = true;
            }
            else
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics resource manager: failed to unload one or more asset resources.");
                return false;
            }
        }

        if (unloaded_count > 0U)
            return true;

        if (found)
            return true;

        TBX_TRACE_ERROR_ONCE("Graphics resource manager: asset resource is not loaded.");
        return false;
    }

    bool GraphicsResourceManager::unload(const Uuid& resource_uuid)
    {
        const GraphicsResourceKey key = make_runtime_resource_key(resource_uuid);
        const GraphicsResourceRecord* record = find_record(key);
        if (record == nullptr)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: resource is not runtime-managed and cannot be "
                "unloaded through the runtime API.");
            return false;
        }

        const auto result = unload_resource(key, *record);
        if (result)
            erase_usage(key);
        else
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: failed to unload runtime resource.");

        return result;
    }

    void GraphicsResourceManager::unload_all()
    {
        auto pinned_assets = std::vector<Handle> {};
        for (const auto& entry : _resources)
        {
            if (!entry.second.usage.asset.is_valid()
                || !should_pin_asset_for_bucket(entry.first.bucket))
            {
                continue;
            }

            auto is_known_asset = false;
            for (const auto& handle : pinned_assets)
            {
                if (handle == entry.second.usage.asset)
                {
                    is_known_asset = true;
                    break;
                }
            }

            if (!is_known_asset)
                pinned_assets.push_back(entry.second.usage.asset);
        }

        _resources.clear();
        _failed_materials.clear();
        _default_texture_resource = nullptr;
        _default_texture = {};
        _default_normal_texture_resource = nullptr;
        _default_normal_texture = {};
        _default_black_texture_resource = nullptr;
        _default_black_texture = {};
        _default_metallic_roughness_texture_resource = nullptr;
        _default_metallic_roughness_texture = {};
        _fallback_material_resource = nullptr;
        _fallback_material_pipeline = {};
        _fallback_material = {};

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
            return;

        for (const auto& handle : pinned_assets)
            asset_manager->set_pinned(handle, false);
    }

    GraphicsResourceKey GraphicsResourceManager::make_asset_resource_key(
        const Uuid asset_id,
        const uint32 bucket)
    {
        return GraphicsResourceKey {.id = asset_id, .bucket = bucket};
    }

    GraphicsResourceKey GraphicsResourceManager::make_runtime_resource_key(const Uuid resource_uuid)
    {
        return GraphicsResourceKey {.id = resource_uuid, .bucket = RUNTIME_RESOURCE_BUCKET};
    }

    bool GraphicsResourceManager::should_pin_asset_for_bucket(const uint32 bucket)
    {
        return bucket == MATERIAL_RESOURCE_BUCKET || bucket == POST_PROCESS_MATERIAL_RESOURCE_BUCKET
               || bucket == TEXTURE_RESOURCE_BUCKET;
    }

    void GraphicsResourceManager::erase_usage(const GraphicsResourceKey& key)
    {
        _resources.erase(key);
    }

    void GraphicsResourceManager::pin_asset_if_tracked(
        const GraphicsResourceKey& key,
        const Handle& handle)
    {
        if (!handle.is_valid() || !should_pin_asset_for_bucket(key.bucket))
            return;

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
            return;

        asset_manager->set_pinned(handle, true);
    }

    void GraphicsResourceManager::unpin_asset_if_unused(
        const GraphicsResourceKey& key,
        const Handle& handle)
    {
        if (!handle.is_valid() || !should_pin_asset_for_bucket(key.bucket))
            return;

        for (const auto& entry : _resources)
        {
            if (entry.first == key)
                continue;
            if (!should_pin_asset_for_bucket(entry.first.bucket))
                continue;
            if (entry.second.usage.asset == handle)
                return;
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
            return;

        asset_manager->set_pinned(handle, false);
    }

    std::optional<GraphicsResourceUsage> GraphicsResourceManager::find_usage(const Handle& handle)
    {
        const Uuid asset_id = resolve_asset_id(handle);
        return _resources.find_usage(
            asset_id,
            {
                MATERIAL_RESOURCE_BUCKET,
                POST_PROCESS_MATERIAL_RESOURCE_BUCKET,
                MODEL_RESOURCE_BUCKET,
                TEXTURE_RESOURCE_BUCKET,
            });
    }

    GraphicsResourceRecord* GraphicsResourceManager::find_record(const GraphicsResourceKey& key)
    {
        return _resources.find(key);
    }

    const GraphicsResourceRecord* GraphicsResourceManager::find_record(
        const GraphicsResourceKey& key) const
    {
        return _resources.find(key);
    }

    std::shared_ptr<AssetManager> GraphicsResourceManager::lock_asset_manager() const
    {
        if (_asset_manager_ref)
            return _asset_manager_ref;

        return _asset_manager.lock();
    }

    std::shared_ptr<IGraphicsBackend> GraphicsResourceManager::lock_backend() const
    {
        if (_backend_ref)
            return _backend_ref;

        return _backend.lock();
    }

    void GraphicsResourceManager::append_parameter_uniform_data(
        const MaterialParameterData& parameter,
        std::vector<Vec4>& out_values)
    {
        std::visit(
            [&out_values](const auto& value)
            {
                using TValue = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<TValue, bool>)
                    out_values.push_back(Vec4(value ? 1.0F : 0.0F, 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, int>)
                    out_values.push_back(Vec4(static_cast<float>(value), 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, float>)
                    out_values.push_back(Vec4(value, 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, double>)
                    out_values.push_back(Vec4(static_cast<float>(value), 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, Vec2>)
                    out_values.push_back(Vec4(value, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, Vec3>)
                    out_values.push_back(Vec4(value, 0.0F));
                else if constexpr (std::is_same_v<TValue, Vec4>)
                    out_values.push_back(value);
                else if constexpr (std::is_same_v<TValue, Color>)
                    out_values.push_back(Vec4(value.r, value.g, value.b, value.a));
                else if constexpr (std::is_same_v<TValue, Mat3>)
                {
                    out_values.push_back(Vec4(value[0], 0.0F));
                    out_values.push_back(Vec4(value[1], 0.0F));
                    out_values.push_back(Vec4(value[2], 0.0F));
                }
                else if constexpr (std::is_same_v<TValue, Mat4>)
                {
                    out_values.push_back(value[0]);
                    out_values.push_back(value[1]);
                    out_values.push_back(value[2]);
                    out_values.push_back(value[3]);
                }
            },
            parameter);
    }

    uint64 GraphicsResourceManager::make_material_key(
        const Uuid pipeline,
        const GraphicsMaterialUniformData& uniforms,
        const std::vector<GraphicsResourceBinding>& textures)
    {
        uint64 hash = fnv1a_hash_uuid(pipeline, TBX_FNV1A_OFFSET_BASIS);
        hash = fnv1a_hash_value(static_cast<uint64>(uniforms.values.size()), hash);
        if (!uniforms.values.empty())
            hash = fnv1a_hash_bytes(uniforms.data(), uniforms.byte_size(), hash);
        for (const auto& texture : textures)
        {
            hash = fnv1a_hash_value(texture.slot, hash);
            hash = fnv1a_hash_uuid(texture.resource, hash);
        }
        return hash == 0U ? 1U : hash;
    }

    GraphicsMaterialUniformData GraphicsResourceManager::make_material_uniform_data(
        const MaterialParameterBindings& parameters)
    {
        auto uniform_data = GraphicsMaterialUniformData {};
        uniform_data.values.reserve(parameters.values.size());
        for (const auto& parameter : parameters)
            append_parameter_uniform_data(parameter.data, uniform_data.values);

        if (uniform_data.values.size() > TBX_MAX_MATERIAL_UNIFORM_VECTORS)
            uniform_data.values.resize(TBX_MAX_MATERIAL_UNIFORM_VECTORS);
        else if (uniform_data.values.size() < TBX_MAX_MATERIAL_UNIFORM_VECTORS)
            uniform_data.values.resize(TBX_MAX_MATERIAL_UNIFORM_VECTORS, Vec4(0.0F));

        return uniform_data;
    }

    bool GraphicsResourceManager::load_material_textures(
        const MaterialTextureBindings& texture_bindings,
        std::vector<GraphicsResourceBinding>& out_textures)
    {
        out_textures.clear();
        out_textures.reserve(texture_bindings.values.size());
        auto used_slots = std::unordered_set<uint32> {};
        auto next_fallback_slot = uint32 {0U};
        for (const auto& texture : texture_bindings)
        {
            auto texture_resource = Uuid {};
            if (texture.texture.is_valid())
            {
                if (const auto result =
                        upload(texture.texture, TextureLoadParameters {}, texture_resource);
                    !result)
                {
                    return result;
                }
            }
            else if (
                const auto result =
                    load_default_texture_for_binding(texture.name, texture_resource);
                !result)
            {
                return result;
            }

            auto resolved_slot = resolve_shader_texture_slot(texture.name);
            if (!resolved_slot.has_value())
            {
                while (used_slots.contains(next_fallback_slot))
                    next_fallback_slot += 1U;
                resolved_slot = next_fallback_slot;
            }

            used_slots.insert(*resolved_slot);
            out_textures.push_back(
                GraphicsResourceBinding {
                    .slot = *resolved_slot,
                    .resource = texture_resource,
                });
        }
        return true;
    }

    bool GraphicsResourceManager::upload_cached_material(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        out_resource_uuid = {};
        if (!handle.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: material handle is invalid.");
            return false;
        }

        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: material asset id is invalid.");
            return false;
        }

        const GraphicsResourceKey key = make_asset_resource_key(asset_id, MATERIAL_RESOURCE_BUCKET);
        if (auto* record = find_record(key); record != nullptr)
        {
            touch_resource(key);
            out_resource_uuid = record->usage.resource;
            return true;
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: asset manager is unavailable.");
            return false;
        }

        const std::shared_ptr<Material> material =
            asset_manager->load<Material>(handle, parameters);
        if (!material)
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: failed to load material asset.");
            return false;
        }

        auto resource_uuid = Uuid {};
        auto resource = std::shared_ptr<GraphicsResource> {};
        if (const auto result =
                upload_material_resource(handle, *material, resource, resource_uuid);
            !result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: failed to upload material resource.");
            return result;
        }

        track_resource(key, handle, resource_uuid, std::move(resource));
        out_resource_uuid = resource_uuid;
        return true;
    }

    bool GraphicsResourceManager::upload_cached_model(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        out_resource_uuid = {};
        if (!handle.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: model handle is invalid.");
            return false;
        }

        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: model asset id is invalid.");
            return false;
        }

        const GraphicsResourceKey key = make_asset_resource_key(asset_id, MODEL_RESOURCE_BUCKET);
        if (auto* record = find_record(key); record != nullptr)
        {
            touch_resource(key);
            out_resource_uuid = record->usage.resource;
            return true;
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: asset manager is unavailable.");
            return false;
        }

        const std::shared_ptr<Model> model = asset_manager->load<Model>(handle, parameters);
        if (!model)
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: failed to load model asset.");
            return false;
        }

        auto resource_uuid = Uuid {};
        auto resource = std::shared_ptr<GraphicsResource> {};
        auto mesh_resources = std::vector<GraphicsModelMeshResource> {};
        if (const auto result = upload_model_resource(
                handle,
                *model,
                resource,
                mesh_resources,
                resource_uuid);
            !result)
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: failed to upload model resource.");
            return result;
        }

        track_resource(key, handle, resource_uuid, std::move(resource), std::move(mesh_resources));
        out_resource_uuid = resource_uuid;
        return true;
    }

    bool GraphicsResourceManager::upload_cached_texture(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        out_resource_uuid = {};
        if (!handle.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: texture handle is invalid.");
            return false;
        }

        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: texture asset id is invalid.");
            return false;
        }

        const GraphicsResourceKey key = make_asset_resource_key(asset_id, TEXTURE_RESOURCE_BUCKET);
        if (auto* record = find_record(key); record != nullptr)
        {
            touch_resource(key);
            out_resource_uuid = record->usage.resource;
            return true;
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: asset manager is unavailable.");
            return false;
        }

        const std::shared_ptr<Texture> texture = asset_manager->load<Texture>(handle, parameters);
        if (!texture)
        {
            TBX_TRACE_ERROR_ONCE("Graphics resource manager: failed to load texture asset.");
            return false;
        }

        auto resource_uuid = Uuid {};
        auto resource = std::shared_ptr<GraphicsResource> {};
        if (const auto result =
                upload_texture_resource(handle, *texture, resource, resource_uuid);
            !result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: failed to upload texture resource.");
            return result;
        }

        track_resource(key, handle, resource_uuid, std::move(resource));

        out_resource_uuid = resource_uuid;
        return true;
    }

    bool GraphicsResourceManager::unload_resource(
        const GraphicsResourceKey& key,
        const GraphicsResourceRecord& record)
    {
        if (!record.resource)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: resource record is missing data.");
            return false;
        }

        unpin_asset_if_unused(key, record.usage.asset);
        return true;
    }

    bool GraphicsResourceManager::upload_material_resource(
        const Handle& handle,
        const Material& material,
        std::shared_ptr<GraphicsResource>& out_resource,
        Uuid& out_resource_uuid)
    {
        for (const auto& texture_binding : material.textures)
        {
            if (!texture_binding.texture.is_valid())
                continue;

            auto texture_resource = Uuid {};
            if (const auto result =
                    upload(texture_binding.texture, TextureLoadParameters {}, texture_resource);
                !result)
            {
                return result;
            }
        }

        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        if (const auto result =
                resource_factory.create_material_resource(handle, material, false, out_resource);
            !result)
        {
            return result;
        }

        out_resource_uuid = out_resource->get_uuid();
        return true;
    }

    bool GraphicsResourceManager::upload_post_process_material_resource(
        const Handle& handle,
        const Material& material,
        std::shared_ptr<GraphicsResource>& out_resource,
        Uuid& out_resource_uuid)
    {
        for (const auto& texture_binding : material.textures)
        {
            if (!texture_binding.texture.is_valid())
                continue;

            auto texture_resource = Uuid {};
            if (const auto result =
                    upload(texture_binding.texture, TextureLoadParameters {}, texture_resource);
                !result)
            {
                return result;
            }
        }

        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        if (const auto result =
                resource_factory.create_material_resource(handle, material, true, out_resource);
            !result)
        {
            return result;
        }

        out_resource_uuid = out_resource->get_uuid();
        return true;
    }

    bool GraphicsResourceManager::load_fallback_material_resource(
        GraphicsMaterialInstanceResource& out_material_resource)
    {
        if (_fallback_material_pipeline.is_valid())
        {
            out_material_resource = _fallback_material;
            return true;
        }

        std::shared_ptr<Material> fallback = make_fallback_material();
        if (!fallback)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: failed to create fallback material.");
            return false;
        }

        auto fallback_resource = std::shared_ptr<GraphicsResource> {};
        if (const auto result = upload_material_resource(
                Handle("Toybox/FallbackMaterial"),
                *fallback,
                fallback_resource,
                _fallback_material_pipeline);
            !result)
        {
            return result;
        }

        _fallback_material_resource = std::move(fallback_resource);
        _fallback_material = GraphicsMaterialInstanceResource {
            .pipeline = _fallback_material_pipeline,
            .parameters = fallback->parameters,
            .textures = fallback->textures,
            .config = fallback->config,
        };
        out_material_resource = _fallback_material;
        return true;
    }

    bool GraphicsResourceManager::upload_model_resource(
        const Handle& handle,
        const Model& model,
        std::shared_ptr<GraphicsResource>& out_resource,
        std::vector<GraphicsModelMeshResource>& out_meshes,
        Uuid& out_resource_uuid)
    {
        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        if (const auto result =
                resource_factory.create_model_resource(handle, model, out_resource, out_meshes);
            !result)
        {
            return result;
        }

        out_resource_uuid = out_resource->get_uuid();
        return true;
    }

    bool GraphicsResourceManager::upload_texture_resource(
        const Handle& handle,
        const Texture& texture,
        std::shared_ptr<GraphicsResource>& out_resource,
        Uuid& out_resource_uuid)
    {
        auto resource_factory = GraphicsResourceFactory(_backend, _asset_manager);
        if (const auto result = resource_factory.create_texture_resource(handle, texture, out_resource);
            !result)
        {
            return result;
        }

        out_resource_uuid = out_resource->get_uuid();
        return true;
    }

    void GraphicsResourceManager::track_resource(
        const GraphicsResourceKey& key,
        const Handle& handle,
        const Uuid resource_uuid,
        std::shared_ptr<GraphicsResource> resource,
        std::any payload)
    {
        _resources.track(
            key,
            handle,
            resource_uuid,
            _current_frame,
            std::move(resource),
            std::move(payload));
        pin_asset_if_tracked(key, handle);
    }

    bool GraphicsResourceManager::touch_resource(const GraphicsResourceKey& key)
    {
        return _resources.touch(key, _current_frame);
    }

    bool GraphicsResourceManager::touch_runtime_resource(const Uuid& resource_uuid)
    {
        return touch_resource(make_runtime_resource_key(resource_uuid));
    }

    bool GraphicsResourceManager::upload_runtime_resource(
        const Handle& handle,
        std::shared_ptr<GraphicsResource> resource,
        Uuid& out_resource_uuid,
        std::any payload)
    {
        out_resource_uuid = {};
        if (!resource)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: resource creation failed.");
            return false;
        }

        out_resource_uuid = resource->get_uuid();
        if (!out_resource_uuid.is_valid())
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics resource manager: resource was not created.");
            return false;
        }

        track_resource(
            make_runtime_resource_key(out_resource_uuid),
            handle,
            out_resource_uuid,
            std::move(resource),
            std::move(payload));
        return true;
    }

    uint GraphicsResourceManager::unload_unused()
    {
        auto unloaded_count = 0U;
        const auto expired_resources = _resources.erase_stale(
            _current_frame,
            _unused_frame_limit,
            [this](const GraphicsResourceKey& key, const GraphicsResourceRecord& record)
            {
                return unload_resource(key, record);
            });
        unloaded_count = static_cast<uint>(expired_resources.size());

        return unloaded_count;
    }

    Uuid GraphicsResourceManager::resolve_asset_id(const Handle& handle)
    {
        const auto asset_manager = lock_asset_manager();
        return asset_manager ? asset_manager->ensure(handle) : Uuid {};
    }
}



