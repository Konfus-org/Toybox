#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/utils/hash.h"
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    static constexpr uint32 MATERIAL_RESOURCE_BUCKET = 1U;
    static constexpr uint32 POST_PROCESS_MATERIAL_RESOURCE_BUCKET = 2U;
    static constexpr uint32 MODEL_RESOURCE_BUCKET = 3U;
    static constexpr uint32 TEXTURE_RESOURCE_BUCKET = 4U;
    static constexpr uint32 RUNTIME_RESOURCE_BUCKET = 5U;

    static bool texture_binding_name_matches(
        const std::string_view binding_name,
        const std::string_view canonical_name)
    {
        if (binding_name == canonical_name)
            return true;

        if (binding_name.size() == canonical_name.size() + 2U && binding_name[0] == 'u'
            && binding_name[1] == '_')
        {
            return binding_name.substr(2U) == canonical_name;
        }

        return false;
    }

    GraphicsResourceManager::GraphicsResourceManager(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        const uint unused_frame_limit)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
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

    Result GraphicsResourceManager::upload(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return upload_cached_material(handle, parameters, out_resource_uuid);
    }

    Result GraphicsResourceManager::upload(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = upload(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    Result GraphicsResourceManager::upload(
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
            return Result(false, "Graphics resource manager: asset manager is unavailable.");

        const std::shared_ptr<Material> material =
            asset_manager->load<Material>(handle, MaterialLoadParameters {});
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

        return {};
    }

    Result GraphicsResourceManager::upload(
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
                return Result(
                    false,
                    "Graphics resource manager: post-process material handle is invalid.");
            }

            const Uuid asset_id = resolve_asset_id(handle);
            if (!asset_id.is_valid())
            {
                return Result(
                    false,
                    "Graphics resource manager: post-process material asset id is invalid.");
            }

            const auto asset_manager = lock_asset_manager();
            if (!asset_manager)
                return Result(false, "Graphics resource manager: asset manager is unavailable.");

            const std::shared_ptr<Material> material =
                asset_manager->load<Material>(handle, MaterialLoadParameters {});
            if (!material)
            {
                return Result(
                    false,
                    "Graphics resource manager: failed to load post-process material asset.");
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
                if (const auto result =
                        upload_post_process_material_resource(handle, *material, pipeline);
                    !result)
                {
                    return result;
                }

                track_resource(key, handle, pipeline);
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
        return {};
    }

    uint GraphicsResourceManager::unload_stale()
    {
        _current_frame += 1U;
        return unload_unused();
    }

    Result GraphicsResourceManager::load_default_texture(Uuid& out_resource_uuid)
    {
        if (_default_texture.is_valid())
        {
            out_resource_uuid = _default_texture;
            return {};
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
                _default_texture);
            !result)
        {
            return result;
        }

        out_resource_uuid = _default_texture;
        return {};
    }

    Result GraphicsResourceManager::ensure_solid_fallback_texture(
        const std::string_view debug_name,
        const Pixel r,
        const Pixel g,
        const Pixel b,
        const Pixel a,
        Uuid& out_resource_uuid)
    {
        if (out_resource_uuid.is_valid())
            return {};

        const auto texture = Texture(
            Size {1U, 1U},
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            TextureFormat::RGBA,
            TextureMipmaps::DISABLED,
            TextureCompression::DISABLED,
            std::vector<Pixel> {r, g, b, a});

        return upload_texture_resource(Handle(std::string(debug_name)), texture, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_default_texture_for_binding(
        const std::string_view binding_name,
        Uuid& out_resource_uuid)
    {
        if (texture_binding_name_matches(binding_name, "normal_map"))
        {
            if (const auto result = ensure_solid_fallback_texture(
                    "Toybox/DefaultNormalTexture",
                    static_cast<Pixel>(128U),
                    static_cast<Pixel>(128U),
                    static_cast<Pixel>(255U),
                    static_cast<Pixel>(255U),
                    _default_normal_texture);
                !result)
            {
                return result;
            }

            out_resource_uuid = _default_normal_texture;
            return {};
        }

        if (texture_binding_name_matches(binding_name, "specular_map")
            || texture_binding_name_matches(binding_name, "emissive_map"))
        {
            if (const auto result = ensure_solid_fallback_texture(
                    "Toybox/DefaultBlackTexture",
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(0U),
                    static_cast<Pixel>(255U),
                    _default_black_texture);
                !result)
            {
                return result;
            }

            out_resource_uuid = _default_black_texture;
            return {};
        }

        return load_default_texture(out_resource_uuid);
    }

    Result GraphicsResourceManager::upload(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return upload_cached_model(handle, parameters, out_resource_uuid);
    }

    Result GraphicsResourceManager::upload(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = upload(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    Result GraphicsResourceManager::upload(
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
            return Result(false, "Graphics resource manager: model resource metadata was missing.");

        const auto* meshes = std::any_cast<std::vector<GraphicsModelMeshResource>>(
            &record->payload);
        if (meshes == nullptr)
            return Result(false, "Graphics resource manager: model resource metadata was invalid.");

        out_model_resource = GraphicsModelResource {
            .asset = handle,
            .resource = record->usage.resource,
            .meshes = *meshes,
        };
        return {};
    }

    Result GraphicsResourceManager::upload(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return upload_cached_texture(handle, parameters, out_resource_uuid);
    }

    Result GraphicsResourceManager::upload(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = upload(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    Result GraphicsResourceManager::upload(
        const GraphicsBufferDesc& desc,
        const void* data,
        const uint64 data_size,
        Uuid& out_resource_uuid)
    {
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return upload_runtime_resource(
            Handle(desc.debug_name),
            out_resource_uuid,
            backend->upload_buffer(desc, data, data_size, out_resource_uuid));
    }

    Result GraphicsResourceManager::upload(
        const GraphicsPipelineDesc& desc,
        Uuid& out_resource_uuid)
    {
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return upload_runtime_resource(
            Handle(desc.debug_name),
            out_resource_uuid,
            backend->upload_pipeline(desc, out_resource_uuid));
    }

    Result GraphicsResourceManager::upload(
        const GraphicsSamplerDesc& desc,
        Uuid& out_resource_uuid)
    {
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return upload_runtime_resource(
            Handle(desc.debug_name),
            out_resource_uuid,
            backend->upload_sampler(desc, out_resource_uuid));
    }

    Result GraphicsResourceManager::upload(
        const GraphicsTextureDesc& desc,
        const void* data,
        const uint64 data_size,
        Uuid& out_resource_uuid)
    {
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return upload_runtime_resource(
            Handle(desc.debug_name),
            out_resource_uuid,
            backend->upload_texture(desc, data, data_size, out_resource_uuid));
    }

    Result GraphicsResourceManager::update(const Uuid& resource_uuid)
    {
        return touch_runtime_resource(resource_uuid)
                   ? Result {}
                   : Result(false, "Graphics resource manager: runtime resource is not tracked.");
    }

    Result GraphicsResourceManager::update(
        const Uuid& resource_uuid,
        const void* data,
        const uint64 data_size,
        const uint64 offset)
    {
        touch_runtime_resource(resource_uuid);
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return backend->update_buffer(resource_uuid, data, data_size, offset);
    }

    Result GraphicsResourceManager::update(
        const Uuid& resource_uuid,
        const GraphicsTextureUpdateDesc& desc,
        const void* data,
        const uint64 data_size)
    {
        touch_runtime_resource(resource_uuid);
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return backend->update_texture(resource_uuid, desc, data, data_size);
    }

    Result GraphicsResourceManager::unload(const Handle& handle)
    {
        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
            return Result(false, "Graphics resource manager: asset id is invalid.");

        const auto buckets = {
            MATERIAL_RESOURCE_BUCKET,
            POST_PROCESS_MATERIAL_RESOURCE_BUCKET,
            MODEL_RESOURCE_BUCKET,
            TEXTURE_RESOURCE_BUCKET,
        };
        auto unloaded_count = 0U;
        auto failure = Result {};
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
            }
            else
            {
                failure = result;
            }
        }

        if (unloaded_count > 0U)
            return {};

        if (!failure)
            return failure;

        return Result(false, "Graphics resource manager: asset resource is not loaded.");
    }

    Result GraphicsResourceManager::unload(const Uuid& resource_uuid)
    {
        const GraphicsResourceKey key = make_runtime_resource_key(resource_uuid);
        const GraphicsResourceRecord* record = find_record(key);
        if (record == nullptr)
        {
            return Result(
                false,
                "Graphics resource manager: resource is not runtime-managed and cannot be "
                "unloaded through the runtime API.");
        }

        const auto result = unload_resource(key, *record);
        if (result)
            erase_usage(key);
        return result;
    }

    void GraphicsResourceManager::unload_all()
    {
        const auto backend = lock_backend();
        if (!backend)
            return;

        for (const auto& entry : _resources)
        {
            if (const auto result = unload_resource(entry.first, entry.second); !result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "GraphicsResourceManager::unload_all: failed to unload managed GPU resource "
                    "({}). {}",
                    entry.second.usage.asset.get_name(),
                    result.get_report());
            }
        }

        _resources.clear();
        _failed_materials.clear();
        if (_default_texture.is_valid())
        {
            backend->unload(_default_texture);
            _default_texture = {};
        }
        if (_default_normal_texture.is_valid())
        {
            backend->unload(_default_normal_texture);
            _default_normal_texture = {};
        }
        if (_default_black_texture.is_valid())
        {
            backend->unload(_default_black_texture);
            _default_black_texture = {};
        }
        if (_fallback_material_pipeline.is_valid())
        {
            backend->unload(_fallback_material_pipeline);
            _fallback_material_pipeline = {};
            _fallback_material = {};
        }
    }

    bool GraphicsResourceManager::append_shader_sources(
        const Handle& handle,
        std::vector<Uuid>& loaded_shader_ids,
        std::vector<ShaderSource>& shader_sources)
    {
        if (!handle.is_valid())
            return true;

        const Uuid asset_id = resolve_asset_id(handle);
        for (const Uuid loaded_shader_id : loaded_shader_ids)
        {
            if (loaded_shader_id == asset_id)
                return true;
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
            return false;

        const std::shared_ptr<Shader> shader =
            asset_manager->load<Shader>(handle, ShaderLoadParameters {});
        if (!shader)
            return false;

        loaded_shader_ids.push_back(asset_id);
        shader_sources.insert(shader_sources.end(), shader->sources.begin(), shader->sources.end());
        return true;
    }

    Result GraphicsResourceManager::build_material_shader(
        const Material& material,
        Shader& out_shader)
    {
        auto shader_sources = std::vector<ShaderSource> {};
        auto loaded_shader_ids = std::vector<Uuid> {};

        if (material.program.compute.is_valid())
        {
            if (!append_shader_sources(material.program.compute, loaded_shader_ids, shader_sources))
                return Result(false, "Graphics resource manager: failed to load compute shader.");
        }
        else
        {
            if (!append_shader_sources(material.program.vertex, loaded_shader_ids, shader_sources))
                return Result(false, "Graphics resource manager: failed to load vertex shader.");
            if (!append_shader_sources(
                    material.program.fragment,
                    loaded_shader_ids,
                    shader_sources))
                return Result(false, "Graphics resource manager: failed to load fragment shader.");
            if (!append_shader_sources(
                    material.program.tesselation,
                    loaded_shader_ids,
                    shader_sources))
                return Result(
                    false,
                    "Graphics resource manager: failed to load tessellation shader.");
            if (!append_shader_sources(
                    material.program.geometry,
                    loaded_shader_ids,
                    shader_sources))
                return Result(false, "Graphics resource manager: failed to load geometry shader.");
        }

        if (shader_sources.empty())
            return Result(false, "Graphics resource manager: material has no shader sources.");

        out_shader = Shader(std::move(shader_sources));
        return {};
    }

    GraphicsResourceKey GraphicsResourceManager::make_asset_resource_key(
        const Uuid asset_id,
        const uint32 bucket)
    {
        return GraphicsResourceKey {.id = asset_id, .bucket = bucket};
    }

    GraphicsResourceKey GraphicsResourceManager::make_runtime_resource_key(
        const Uuid resource_uuid)
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
        return _asset_manager.lock();
    }

    std::shared_ptr<IGraphicsBackend> GraphicsResourceManager::lock_backend() const
    {
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

    Result GraphicsResourceManager::load_material_textures(
        const MaterialTextureBindings& texture_bindings,
        std::vector<GraphicsResourceBinding>& out_textures)
    {
        out_textures.clear();
        auto texture_slot = uint32 {0U};
        out_textures.reserve(texture_bindings.values.size());
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
            else if (const auto result =
                         load_default_texture_for_binding(texture.name, texture_resource);
                     !result)
            {
                return result;
            }

            out_textures.push_back(
                GraphicsResourceBinding {
                    .slot = texture_slot,
                    .resource = texture_resource,
                });
            texture_slot += 1U;
        }
        return {};
    }

    Result GraphicsResourceManager::upload_cached_material(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        out_resource_uuid = {};
        if (!handle.is_valid())
            return Result(false, "Graphics resource manager: material handle is invalid.");

        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
            return Result(false, "Graphics resource manager: material asset id is invalid.");

        const GraphicsResourceKey key = make_asset_resource_key(asset_id, MATERIAL_RESOURCE_BUCKET);
        if (auto* record = find_record(key); record != nullptr)
        {
            touch_resource(key);
            out_resource_uuid = record->usage.resource;
            return {};
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
            return Result(false, "Graphics resource manager: asset manager is unavailable.");

        const std::shared_ptr<Material> material =
            asset_manager->load<Material>(handle, parameters);
        if (!material)
            return Result(false, "Graphics resource manager: failed to load material asset.");

        auto resource_uuid = Uuid {};
        if (const auto result = upload_material_resource(handle, *material, resource_uuid); !result)
            return result;

        track_resource(key, handle, resource_uuid);
        out_resource_uuid = resource_uuid;
        return {};
    }

    Result GraphicsResourceManager::upload_cached_model(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        out_resource_uuid = {};
        if (!handle.is_valid())
            return Result(false, "Graphics resource manager: model handle is invalid.");

        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
            return Result(false, "Graphics resource manager: model asset id is invalid.");

        const GraphicsResourceKey key = make_asset_resource_key(asset_id, MODEL_RESOURCE_BUCKET);
        if (auto* record = find_record(key); record != nullptr)
        {
            touch_resource(key);
            out_resource_uuid = record->usage.resource;
            return {};
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
            return Result(false, "Graphics resource manager: asset manager is unavailable.");

        const std::shared_ptr<Model> model = asset_manager->load<Model>(handle, parameters);
        if (!model)
            return Result(false, "Graphics resource manager: failed to load model asset.");

        auto resource_uuid = Uuid {};
        auto backend_resources = std::vector<Uuid> {};
        auto mesh_resources = std::vector<GraphicsModelMeshResource> {};
        if (const auto result = upload_model_resource(
                handle,
                *model,
                backend_resources,
                mesh_resources,
                resource_uuid);
            !result)
            return result;

        track_resource(
            key,
            handle,
            resource_uuid,
            std::move(backend_resources),
            std::move(mesh_resources));
        out_resource_uuid = resource_uuid;
        return {};
    }

    Result GraphicsResourceManager::upload_cached_texture(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        out_resource_uuid = {};
        if (!handle.is_valid())
            return Result(false, "Graphics resource manager: texture handle is invalid.");

        const Uuid asset_id = resolve_asset_id(handle);
        if (!asset_id.is_valid())
            return Result(false, "Graphics resource manager: texture asset id is invalid.");

        const GraphicsResourceKey key = make_asset_resource_key(asset_id, TEXTURE_RESOURCE_BUCKET);
        if (auto* record = find_record(key); record != nullptr)
        {
            touch_resource(key);
            out_resource_uuid = record->usage.resource;
            return {};
        }

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
            return Result(false, "Graphics resource manager: asset manager is unavailable.");

        const std::shared_ptr<Texture> texture = asset_manager->load<Texture>(handle, parameters);
        if (!texture)
            return Result(false, "Graphics resource manager: failed to load texture asset.");

        auto resource_uuid = Uuid {};
        if (const auto result = upload_texture_resource(handle, *texture, resource_uuid); !result)
            return result;

        track_resource(key, handle, resource_uuid);

        out_resource_uuid = resource_uuid;
        return {};
    }

    Result GraphicsResourceManager::unload_backend_resources(const GraphicsResourceRecord& record)
    {
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        if (record.backend_resources.empty())
            return backend->unload(record.usage.resource);

        for (const Uuid resource : record.backend_resources)
        {
            if (const auto result = backend->unload(resource); !result)
                return result;
        }

        return {};
    }

    Result GraphicsResourceManager::unload_resource(
        const GraphicsResourceKey& key,
        const GraphicsResourceRecord& record)
    {
        const auto result = unload_backend_resources(record);
        if (result)
            unpin_asset_if_unused(key, record.usage.asset);
        return result;
    }

    Result GraphicsResourceManager::upload_material_resource(
        const Handle& handle,
        const Material& material,
        Uuid& out_resource_uuid)
    {
        auto shader = Shader {};
        if (const auto result = build_material_shader(material, shader); !result)
            return result;

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

        const auto desc = make_material_pipeline_desc(material, std::move(shader), handle);
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return backend->upload_pipeline(desc, out_resource_uuid);
    }

    Result GraphicsResourceManager::upload_post_process_material_resource(
        const Handle& handle,
        const Material& material,
        Uuid& out_resource_uuid)
    {
        auto shader = Shader {};
        if (const auto result = build_material_shader(material, shader); !result)
            return result;

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

        const auto desc = make_post_process_pipeline_desc(std::move(shader), handle);
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return backend->upload_pipeline(desc, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_fallback_material_resource(
        GraphicsMaterialInstanceResource& out_material_resource)
    {
        if (_fallback_material_pipeline.is_valid())
        {
            out_material_resource = _fallback_material;
            return {};
        }

        std::shared_ptr<Material> fallback = make_fallback_material();
        if (!fallback)
            return Result(false, "Graphics resource manager: failed to create fallback material.");

        if (const auto result = upload_material_resource(
                Handle("Toybox/FallbackMaterial"),
                *fallback,
                _fallback_material_pipeline);
            !result)
        {
            return result;
        }

        _fallback_material = GraphicsMaterialInstanceResource {
            .pipeline = _fallback_material_pipeline,
            .parameters = fallback->parameters,
            .textures = fallback->textures,
            .config = fallback->config,
        };
        out_material_resource = _fallback_material;
        return {};
    }

    Result GraphicsResourceManager::upload_model_resource(
        const Handle& handle,
        const Model& model,
        std::vector<Uuid>& out_backend_resources,
        std::vector<GraphicsModelMeshResource>& out_meshes,
        Uuid& out_resource_uuid)
    {
        out_backend_resources.clear();
        out_meshes.clear();
        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        for (uint mesh_index = 0U; mesh_index < static_cast<uint>(model.meshes.size());
             ++mesh_index)
        {
            const Mesh& mesh = model.meshes[static_cast<size>(mesh_index)];
            auto mesh_resource = GraphicsModelMeshResource {
                .index_count = static_cast<uint32>(mesh.indices.size()),
            };
            if (mesh.bounds.is_valid)
            {
                mesh_resource.local_bounds = mesh.bounds.sphere;
                mesh_resource.has_local_bounds = true;
            }

            if (!mesh.vertices.empty())
            {
                const uint64 vertex_data_size =
                    static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
                auto vertex_buffer = Uuid {};
                if (const auto result = backend->upload_buffer(
                        make_model_vertex_buffer_desc(handle, mesh_index, vertex_data_size),
                        mesh.vertices.data(),
                        vertex_data_size,
                        vertex_buffer);
                    !result)
                {
                    for (const Uuid resource : out_backend_resources)
                        backend->unload(resource);
                    out_backend_resources.clear();
                    out_meshes.clear();
                    return result;
                }
                out_backend_resources.push_back(vertex_buffer);
                mesh_resource.vertex_buffer = vertex_buffer;
            }

            if (!mesh.indices.empty())
            {
                const uint64 index_data_size =
                    static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));
                auto index_buffer = Uuid {};
                if (const auto result = backend->upload_buffer(
                        make_model_index_buffer_desc(handle, mesh_index, index_data_size),
                        mesh.indices.data(),
                        index_data_size,
                        index_buffer);
                    !result)
                {
                    for (const Uuid resource : out_backend_resources)
                        backend->unload(resource);
                    out_backend_resources.clear();
                    out_meshes.clear();
                    return result;
                }
                out_backend_resources.push_back(index_buffer);
                mesh_resource.index_buffer = index_buffer;
            }

            if (mesh_resource.vertex_buffer.is_valid() && mesh_resource.index_buffer.is_valid()
                && mesh_resource.index_count > 0U)
            {
                out_meshes.push_back(mesh_resource);
            }
        }

        if (out_meshes.empty())
            return Result(false, "Graphics resource manager: model has no mesh data to upload.");

        out_resource_uuid = Uuid::generate();
        return {};
    }

    Result GraphicsResourceManager::upload_texture_resource(
        const Handle& handle,
        const Texture& texture,
        Uuid& out_resource_uuid)
    {
        if (texture.resolution.width == 0U || texture.resolution.height == 0U)
            return Result(false, "Graphics resource manager: texture size is invalid.");

        const uint64 source_byte_size = get_texture_source_byte_size(texture);
        if (source_byte_size > 0U && static_cast<uint64>(texture.pixels.size()) < source_byte_size)
        {
            return Result(
                false,
                "Graphics resource manager: texture pixel data is smaller than expected.");
        }

        const auto desc = make_texture_desc(texture, handle);
        const void* upload_data = nullptr;
        uint64 upload_data_size = 0U;
        auto expanded_pixels = std::vector<Pixel> {};
        if (!texture.pixels.empty())
        {
            if (texture.format == TextureFormat::RGB)
            {
                expanded_pixels = expand_rgb_to_rgba(texture);
                upload_data = expanded_pixels.data();
                upload_data_size = static_cast<uint64>(expanded_pixels.size());
            }
            else
            {
                upload_data = texture.pixels.data();
                upload_data_size = get_texture_upload_byte_size(texture);
            }
        }

        const auto backend = lock_backend();
        if (!backend)
            return Result(false, "Graphics resource manager: graphics backend is unavailable.");

        return backend->upload_texture(desc, upload_data, upload_data_size, out_resource_uuid);
    }

    void GraphicsResourceManager::track_resource(
        const GraphicsResourceKey& key,
        const Handle& handle,
        const Uuid resource_uuid,
        std::vector<Uuid> backend_resources,
        std::any payload)
    {
        _resources.track(
            key,
            handle,
            resource_uuid,
            _current_frame,
            std::move(backend_resources),
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

    Result GraphicsResourceManager::upload_runtime_resource(
        const Handle& handle,
        Uuid& out_resource_uuid,
        const Result& upload_result)
    {
        if (!upload_result)
            return upload_result;

        track_resource(make_runtime_resource_key(out_resource_uuid), handle, out_resource_uuid);
        return upload_result;
    }

    uint GraphicsResourceManager::unload_unused()
    {
        auto unloaded_count = 0U;
        auto expired_resources = _resources.get_stale(_current_frame, _unused_frame_limit);

        for (const auto& key : expired_resources)
        {
            const auto* record = find_record(key);
            if (record == nullptr)
                continue;

            if (const auto result = unload_resource(key, *record); result)
            {
                unloaded_count += 1U;
                erase_usage(key);
            }
        }

        return unloaded_count;
    }

    Uuid GraphicsResourceManager::resolve_asset_id(const Handle& handle)
    {
        const auto asset_manager = lock_asset_manager();
        return asset_manager ? asset_manager->ensure(handle) : Uuid {};
    }

    GraphicsTextureDesc GraphicsResourceManager::make_texture_desc(
        const Texture& texture,
        const Handle& handle)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED,
            .format = GraphicsTextureFormat::RGBA8,
            .size = texture.resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = std::string("Texture ") + to_string(handle),
        };
    }

    GraphicsBufferDesc GraphicsResourceManager::make_model_index_buffer_desc(
        const Handle& handle,
        const uint mesh_index,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::INDEX,
            .size = byte_size,
            .is_dynamic = false,
            .debug_name = std::string("Model ") + to_string(handle) + " Mesh "
                          + std::to_string(mesh_index) + " Indices",
        };
    }

    GraphicsBufferDesc GraphicsResourceManager::make_model_vertex_buffer_desc(
        const Handle& handle,
        const uint mesh_index,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::VERTEX,
            .size = byte_size,
            .is_dynamic = false,
            .debug_name = std::string("Model ") + to_string(handle) + " Mesh "
                          + std::to_string(mesh_index) + " Vertices",
        };
    }

    static GraphicsVertexFormat to_graphics_vertex_format(const VertexData& data)
    {
        if (std::holds_alternative<float>(data))
            return GraphicsVertexFormat::FLOAT;
        if (std::holds_alternative<Vec2>(data))
            return GraphicsVertexFormat::VEC2;
        if (std::holds_alternative<Vec3>(data))
            return GraphicsVertexFormat::VEC3;
        if (std::holds_alternative<Vec4>(data) || std::holds_alternative<Color>(data))
            return GraphicsVertexFormat::VEC4;
        if (std::holds_alternative<int>(data))
            return GraphicsVertexFormat::INT32;

        return GraphicsVertexFormat::FLOAT;
    }

    static uint32 get_vertex_attribute_location(const VertexAttributeSemantic semantic)
    {
        switch (semantic)
        {
            case VertexAttributeSemantic::POSITION:
                return 0U;
            case VertexAttributeSemantic::COLOR:
                return 1U;
            case VertexAttributeSemantic::NORMAL:
                return 2U;
            case VertexAttributeSemantic::UV:
                return 3U;
            case VertexAttributeSemantic::TANGENT:
                return 4U;
            case VertexAttributeSemantic::NONE:
            default:
                return 0U;
        }
    }

    static void append_vertex_layout_attributes(
        const VertexBufferLayout& layout,
        std::vector<GraphicsVertexAttributeDesc>& out_attributes)
    {
        for (const auto& attribute : layout.elements)
        {
            if (attribute.semantic == VertexAttributeSemantic::NONE)
                continue;

            out_attributes.push_back(
                GraphicsVertexAttributeDesc {
                    .location = get_vertex_attribute_location(attribute.semantic),
                    .buffer_slot = 0U,
                    .offset = attribute.offset,
                    .format = to_graphics_vertex_format(attribute.type),
                });
        }
    }

    static void append_instance_layout_attributes(
        std::vector<GraphicsVertexAttributeDesc>& out_attributes)
    {
        for (uint32 column = 0U; column < 4U; ++column)
        {
            out_attributes.push_back(
                GraphicsVertexAttributeDesc {
                    .location = 5U + column,
                    .buffer_slot = 1U,
                    .offset = static_cast<uint32>(sizeof(float) * 4U * column),
                    .format = GraphicsVertexFormat::VEC4,
                });
        }
    }

    GraphicsPipelineDesc GraphicsResourceManager::make_material_pipeline_desc(
        const Material& material,
        Shader shader,
        const Handle& handle)
    {
        const VertexBufferLayout vertex_layout = get_default_vertex_buffer_layout();
        auto vertex_attributes = std::vector<GraphicsVertexAttributeDesc> {};
        append_vertex_layout_attributes(vertex_layout, vertex_attributes);
        append_instance_layout_attributes(vertex_attributes);

        return GraphicsPipelineDesc {
            .shader = std::move(shader),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = vertex_layout.stride,
                    },
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 1U,
                        .stride = static_cast<uint32>(sizeof(Mat4)),
                        .is_per_instance = true,
                    },
                },
            .vertex_attributes = std::move(vertex_attributes),
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = material.config.is_depth_test_enabled,
            .is_depth_write_enabled = material.config.is_depth_write_enabled,
            .is_blending_enabled = material.config.blend_mode == MaterialBlendMode::AlphaBlend,
            .is_culling_enabled = material.config.is_cullable && !material.config.is_two_sided,
            .debug_name = std::string("Material ") + to_string(handle),
        };
    }

    GraphicsPipelineDesc GraphicsResourceManager::make_post_process_pipeline_desc(
        Shader shader,
        const Handle& handle)
    {
        const VertexBufferLayout vertex_layout = get_default_vertex_buffer_layout();
        auto vertex_attributes = std::vector<GraphicsVertexAttributeDesc> {};
        append_vertex_layout_attributes(vertex_layout, vertex_attributes);

        return GraphicsPipelineDesc {
            .shader = std::move(shader),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = vertex_layout.stride,
                    },
                },
            .vertex_attributes = std::move(vertex_attributes),
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = false,
            .is_depth_write_enabled = false,
            .is_blending_enabled = false,
            .is_culling_enabled = false,
            .debug_name = std::string("Post Process Material ") + to_string(handle),
        };
    }

    std::vector<Pixel> GraphicsResourceManager::expand_rgb_to_rgba(const Texture& texture)
    {
        auto pixels = std::vector<Pixel> {};
        pixels.reserve(static_cast<size>(get_texture_upload_byte_size(texture)));

        const uint64 pixel_data_size = static_cast<uint64>(texture.pixels.size());
        for (uint64 source_index = 0U; source_index + 2U < pixel_data_size; source_index += 3U)
        {
            pixels.push_back(texture.pixels[static_cast<size>(source_index)]);
            pixels.push_back(texture.pixels[static_cast<size>(source_index + 1U)]);
            pixels.push_back(texture.pixels[static_cast<size>(source_index + 2U)]);
            pixels.push_back(static_cast<Pixel>(255U));
        }

        return pixels;
    }

    uint64 GraphicsResourceManager::get_texture_channel_count(const TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGB:
                return 3U;
            case TextureFormat::RGBA:
                return 4U;
            default:
                return 4U;
        }
    }

    uint64 GraphicsResourceManager::get_texture_pixel_count(const Texture& texture)
    {
        return static_cast<uint64>(texture.resolution.width)
               * static_cast<uint64>(texture.resolution.height);
    }

    uint64 GraphicsResourceManager::get_texture_source_byte_size(const Texture& texture)
    {
        return get_texture_pixel_count(texture) * get_texture_channel_count(texture.format);
    }

    uint64 GraphicsResourceManager::get_texture_upload_byte_size(const Texture& texture)
    {
        return get_texture_pixel_count(texture) * 4U;
    }
}
