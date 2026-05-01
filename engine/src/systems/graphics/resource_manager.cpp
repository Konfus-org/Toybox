#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/assets/fallbacks.h"
#include <string>
#include <utility>
#include <vector>

namespace tbx
{
    GraphicsResourceManager::GraphicsResourceManager(
        IGraphicsBackend& backend,
        AssetManager& asset_manager,
        const uint unused_frame_limit)
        : _backend(backend)
        , _asset_manager(asset_manager)
        , _unused_frame_limit(unused_frame_limit)
    {
    }

    GraphicsResourceManager::~GraphicsResourceManager() noexcept
    {
        unload_all();
    }

    uint GraphicsResourceManager::update()
    {
        _current_frame += 1U;
        return unload_unused();
    }

    bool GraphicsResourceManager::is_loaded(const Handle& handle)
    {
        return find_usage(handle).has_value();
    }

    std::optional<GraphicsResourceUsage> GraphicsResourceManager::get_usage(const Handle& handle)
    {
        return find_usage(handle);
    }

    Result GraphicsResourceManager::load_material(const Handle& handle, Uuid& out_resource_uuid)
    {
        return load_material(handle, MaterialLoadParameters {}, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_material(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return load_cached_material(handle, parameters, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_material_instance(
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
        if (const auto result = load_material(handle, pipeline_resource); !result)
        {
            _failed_materials.insert(asset_id);
            return load_fallback_material_resource(out_material_resource);
        }

        const auto material_iterator = _material_resources.find(asset_id);
        if (material_iterator == _material_resources.end())
        {
            _failed_materials.insert(asset_id);
            return load_fallback_material_resource(out_material_resource);
        }

        out_material_resource = material_iterator->second;
        out_material_resource.pipeline = pipeline_resource;
        out_material_resource.config = instance.has_config_override_enabled()
                                           ? instance.config
                                           : out_material_resource.config;

        for (const auto& parameter : instance.param_overrides)
            out_material_resource.parameters.set(parameter);

        for (const auto& texture : instance.texture_overrides)
            out_material_resource.textures.set(texture);

        for (const auto& texture : out_material_resource.textures)
        {
            if (!texture.texture.is_valid())
                continue;

            auto texture_resource = Uuid {};
            if (const auto result = load_texture(texture.texture, texture_resource); !result)
                return result;
        }

        return {};
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

    Result GraphicsResourceManager::load_material(const Handle& handle, uint& out_gpu_handle)
    {
        return load_material(handle, MaterialLoadParameters {}, out_gpu_handle);
    }

    Result GraphicsResourceManager::load_material(
        const Handle& handle,
        const MaterialLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = load_material(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    bool GraphicsResourceManager::unload_material(const Handle& handle)
    {
        const Uuid asset_id = resolve_asset_id(handle);
        auto iterator = _materials.find(asset_id);
        if (iterator == _materials.end())
            return false;

        const auto result = unload_backend_resources(asset_id, iterator->second);
        if (!result)
            return false;

        erase_usage(asset_id, _materials, _material_last_access_frames);
        _material_resources.erase(asset_id);
        return true;
    }

    Result GraphicsResourceManager::load_model(const Handle& handle, Uuid& out_resource_uuid)
    {
        return load_model(handle, ModelLoadParameters {}, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_model(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return load_cached_model(handle, parameters, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_model(const Handle& handle, uint& out_gpu_handle)
    {
        return load_model(handle, ModelLoadParameters {}, out_gpu_handle);
    }

    Result GraphicsResourceManager::load_model(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = load_model(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    Result GraphicsResourceManager::load_model(
        const Handle& handle,
        GraphicsModelResource& out_model_resource)
    {
        return load_model(handle, ModelLoadParameters {}, out_model_resource);
    }

    Result GraphicsResourceManager::load_model(
        const Handle& handle,
        const ModelLoadParameters& parameters,
        GraphicsModelResource& out_model_resource)
    {
        out_model_resource = {};
        auto resource_uuid = Uuid {};
        if (const auto result = load_model(handle, parameters, resource_uuid); !result)
            return result;

        const Uuid asset_id = resolve_asset_id(handle);
        const auto iterator = _model_resources.find(asset_id);
        if (iterator == _model_resources.end())
            return Result(false, "Graphics resource manager: model resource metadata was missing.");

        out_model_resource = iterator->second;
        return {};
    }

    bool GraphicsResourceManager::unload_model(const Handle& handle)
    {
        const Uuid asset_id = resolve_asset_id(handle);
        auto iterator = _models.find(asset_id);
        if (iterator == _models.end())
            return false;

        const auto result = unload_backend_resources(asset_id, iterator->second);
        if (!result)
            return false;

        erase_usage(asset_id, _models, _model_last_access_frames);
        _model_resources.erase(asset_id);
        _model_backend_resources.erase(asset_id);
        return true;
    }

    Result GraphicsResourceManager::load_texture(const Handle& handle, Uuid& out_resource_uuid)
    {
        return load_texture(handle, TextureLoadParameters {}, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_texture(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        Uuid& out_resource_uuid)
    {
        return load_cached_texture(handle, parameters, out_resource_uuid);
    }

    Result GraphicsResourceManager::load_texture(const Handle& handle, uint& out_gpu_handle)
    {
        return load_texture(handle, TextureLoadParameters {}, out_gpu_handle);
    }

    Result GraphicsResourceManager::load_texture(
        const Handle& handle,
        const TextureLoadParameters& parameters,
        uint& out_gpu_handle)
    {
        auto resource_uuid = Uuid {};
        const auto result = load_texture(handle, parameters, resource_uuid);
        out_gpu_handle = static_cast<uint>(resource_uuid);
        return result;
    }

    bool GraphicsResourceManager::unload_texture(const Handle& handle)
    {
        const Uuid asset_id = resolve_asset_id(handle);
        auto iterator = _textures.find(asset_id);
        if (iterator == _textures.end())
            return false;

        const auto result = unload_backend_resources(asset_id, iterator->second);
        if (!result)
            return false;

        erase_usage(asset_id, _textures, _texture_last_access_frames);
        return true;
    }

    void GraphicsResourceManager::set_unused_frame_limit(const uint unused_frame_limit)
    {
        _unused_frame_limit = unused_frame_limit;
    }

    void GraphicsResourceManager::unload_all()
    {
        for (const auto& entry : _materials)
            unload_backend_resources(entry.first, entry.second);
        for (const auto& entry : _models)
            unload_backend_resources(entry.first, entry.second);
        for (const auto& entry : _textures)
            unload_backend_resources(entry.first, entry.second);

        _materials.clear();
        _material_resources.clear();
        _material_last_access_frames.clear();
        _failed_materials.clear();
        _models.clear();
        _model_resources.clear();
        _model_last_access_frames.clear();
        _model_backend_resources.clear();
        _textures.clear();
        _texture_last_access_frames.clear();
        if (_default_texture.is_valid())
        {
            _backend.unload(_default_texture);
            _default_texture = {};
        }
        if (_fallback_material_pipeline.is_valid())
        {
            _backend.unload(_fallback_material_pipeline);
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

        const std::shared_ptr<Shader> shader = _asset_manager.load<Shader>(handle);
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

    void GraphicsResourceManager::erase_usage(
        const Uuid asset_id,
        std::unordered_map<Uuid, GraphicsResourceUsage>& resources,
        std::unordered_map<Uuid, uint>& last_access_frames)
    {
        resources.erase(asset_id);
        last_access_frames.erase(asset_id);
    }

    std::optional<GraphicsResourceUsage> GraphicsResourceManager::find_usage(const Handle& handle)
    {
        const Uuid asset_id = resolve_asset_id(handle);
        const auto material_iterator = _materials.find(asset_id);
        if (material_iterator != _materials.end())
            return material_iterator->second;

        const auto model_iterator = _models.find(asset_id);
        if (model_iterator != _models.end())
            return model_iterator->second;

        const auto texture_iterator = _textures.find(asset_id);
        if (texture_iterator != _textures.end())
            return texture_iterator->second;

        return std::nullopt;
    }

    Result GraphicsResourceManager::load_cached_material(
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

        auto iterator = _materials.find(asset_id);
        if (iterator != _materials.end())
        {
            auto& usage = iterator->second;
            usage.access_count += 1U;
            _material_last_access_frames[asset_id] = _current_frame;
            out_resource_uuid = usage.resource;
            return {};
        }

        const std::shared_ptr<Material> material =
            _asset_manager.load<Material>(handle, parameters);
        if (!material)
            return Result(false, "Graphics resource manager: failed to load material asset.");

        auto resource_uuid = Uuid {};
        if (const auto result = upload_material_resource(handle, *material, resource_uuid); !result)
            return result;

        _materials.emplace(
            asset_id,
            GraphicsResourceUsage {
                .asset = handle,
                .resource = resource_uuid,
                .access_count = 1U,
            });
        _material_resources[asset_id] = GraphicsMaterialInstanceResource {
            .material = handle,
            .pipeline = resource_uuid,
            .parameters = material->parameters,
            .textures = material->textures,
            .config = material->config,
        };
        _material_last_access_frames[asset_id] = _current_frame;
        out_resource_uuid = resource_uuid;
        return {};
    }

    Result GraphicsResourceManager::load_cached_model(
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

        auto iterator = _models.find(asset_id);
        if (iterator != _models.end())
        {
            auto& usage = iterator->second;
            usage.access_count += 1U;
            _model_last_access_frames[asset_id] = _current_frame;
            out_resource_uuid = usage.resource;
            return {};
        }

        const std::shared_ptr<Model> model = _asset_manager.load<Model>(handle, parameters);
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

        _models.emplace(
            asset_id,
            GraphicsResourceUsage {
                .asset = handle,
                .resource = resource_uuid,
                .access_count = 1U,
            });
        _model_resources[asset_id] = GraphicsModelResource {
            .asset = handle,
            .resource = resource_uuid,
            .meshes = std::move(mesh_resources),
        };
        _model_backend_resources[asset_id] = std::move(backend_resources);
        _model_last_access_frames[asset_id] = _current_frame;
        out_resource_uuid = resource_uuid;
        return {};
    }

    Result GraphicsResourceManager::load_cached_texture(
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

        auto iterator = _textures.find(asset_id);
        if (iterator != _textures.end())
        {
            auto& usage = iterator->second;
            usage.access_count += 1U;
            _texture_last_access_frames[asset_id] = _current_frame;
            out_resource_uuid = usage.resource;
            return {};
        }

        const std::shared_ptr<Texture> texture = _asset_manager.load<Texture>(handle, parameters);
        if (!texture)
            return Result(false, "Graphics resource manager: failed to load texture asset.");

        auto resource_uuid = Uuid {};
        if (const auto result = upload_texture_resource(handle, *texture, resource_uuid); !result)
            return result;

        _textures.emplace(
            asset_id,
            GraphicsResourceUsage {
                .asset = handle,
                .resource = resource_uuid,
                .access_count = 1U,
            });
        _texture_last_access_frames[asset_id] = _current_frame;

        out_resource_uuid = resource_uuid;
        return {};
    }

    Result GraphicsResourceManager::unload_backend_resources(
        const Uuid asset_id,
        const GraphicsResourceUsage& usage)
    {
        auto model_resources = _model_backend_resources.find(asset_id);
        if (model_resources == _model_backend_resources.end())
            return _backend.unload(usage.resource);

        for (const Uuid resource : model_resources->second)
        {
            if (const auto result = _backend.unload(resource); !result)
                return result;
        }

        return {};
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
            if (const auto result = load_texture(texture_binding.texture, texture_resource);
                !result)
                return result;
        }

        const auto desc = make_material_pipeline_desc(material, std::move(shader), handle);
        return _backend.upload_pipeline(desc, out_resource_uuid);
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

        fallback->textures.set("diffuse_map", {});
        fallback->textures.set("normal_map", {});
        fallback->textures.set("specular_map", {});
        fallback->textures.set("shininess_map", {});
        fallback->textures.set("emissive_map", {});

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
        for (uint mesh_index = 0U; mesh_index < static_cast<uint>(model.meshes.size());
             ++mesh_index)
        {
            const Mesh& mesh = model.meshes[static_cast<size>(mesh_index)];
            auto mesh_resource = GraphicsModelMeshResource {
                .index_count = static_cast<uint32>(mesh.indices.size()),
            };

            if (!mesh.vertices.empty())
            {
                const uint64 vertex_data_size =
                    static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
                auto vertex_buffer = Uuid {};
                if (const auto result = _backend.upload_buffer(
                        make_model_vertex_buffer_desc(handle, mesh_index, vertex_data_size),
                        mesh.vertices.data(),
                        vertex_data_size,
                        vertex_buffer);
                    !result)
                {
                    for (const Uuid resource : out_backend_resources)
                        _backend.unload(resource);
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
                if (const auto result = _backend.upload_buffer(
                        make_model_index_buffer_desc(handle, mesh_index, index_data_size),
                        mesh.indices.data(),
                        index_data_size,
                        index_buffer);
                    !result)
                {
                    for (const Uuid resource : out_backend_resources)
                        _backend.unload(resource);
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

        return _backend.upload_texture(desc, upload_data, upload_data_size, out_resource_uuid);
    }

    uint GraphicsResourceManager::unload_unused()
    {
        uint unloaded_count = unload_unused(_materials, _material_last_access_frames);
        unloaded_count += unload_unused(_models, _model_last_access_frames);
        unloaded_count += unload_unused(_textures, _texture_last_access_frames);
        return unloaded_count;
    }

    uint GraphicsResourceManager::unload_unused(
        std::unordered_map<Uuid, GraphicsResourceUsage>& resources,
        std::unordered_map<Uuid, uint>& last_access_frames)
    {
        auto unloaded_count = 0U;
        auto expired_assets = std::vector<Uuid> {};

        for (const auto& entry : resources)
        {
            auto frame_iterator = last_access_frames.find(entry.first);
            const uint last_access_frame =
                frame_iterator == last_access_frames.end() ? 0U : frame_iterator->second;
            if (_current_frame < last_access_frame)
                continue;

            const uint frame_age = _current_frame - last_access_frame;
            if (frame_age < _unused_frame_limit)
                continue;

            if (const auto result = unload_backend_resources(entry.first, entry.second); result)
            {
                expired_assets.push_back(entry.first);
                unloaded_count += 1U;
            }
        }

        for (const Uuid asset_id : expired_assets)
        {
            erase_usage(asset_id, resources, last_access_frames);
            _material_resources.erase(asset_id);
            _model_resources.erase(asset_id);
            _model_backend_resources.erase(asset_id);
        }

        return unloaded_count;
    }

    Uuid GraphicsResourceManager::resolve_asset_id(const Handle& handle)
    {
        return _asset_manager.ensure(handle);
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

    GraphicsPipelineDesc GraphicsResourceManager::make_material_pipeline_desc(
        const Material& material,
        Shader shader,
        const Handle& handle)
    {
        return GraphicsPipelineDesc {
            .shader = std::move(shader),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = static_cast<uint32>(sizeof(float) * 16U),
                    },
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 1U,
                        .stride = static_cast<uint32>(sizeof(Mat4)),
                        .is_per_instance = true,
                    },
                },
            .vertex_attributes =
                {
                    GraphicsVertexAttributeDesc {
                        .location = 0U,
                        .buffer_slot = 0U,
                        .offset = 0U,
                        .format = GraphicsVertexFormat::VEC3,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 1U,
                        .buffer_slot = 0U,
                        .offset = static_cast<uint32>(sizeof(float) * 3U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 2U,
                        .buffer_slot = 0U,
                        .offset = static_cast<uint32>(sizeof(float) * 7U),
                        .format = GraphicsVertexFormat::VEC3,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 3U,
                        .buffer_slot = 0U,
                        .offset = static_cast<uint32>(sizeof(float) * 10U),
                        .format = GraphicsVertexFormat::VEC2,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 4U,
                        .buffer_slot = 0U,
                        .offset = static_cast<uint32>(sizeof(float) * 12U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 5U,
                        .buffer_slot = 1U,
                        .offset = 0U,
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 6U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 4U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 7U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 8U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 8U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 12U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                },
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = material.config.is_depth_test_enabled,
            .is_depth_write_enabled = material.config.is_depth_write_enabled,
            .is_blending_enabled = material.config.blend_mode == MaterialBlendMode::AlphaBlend,
            .is_culling_enabled = material.config.is_cullable && !material.config.is_two_sided,
            .debug_name = std::string("Material ") + to_string(handle),
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
