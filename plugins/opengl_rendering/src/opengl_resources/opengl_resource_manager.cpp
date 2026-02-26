#include "opengl_resource_manager.h"
#include "opengl_buffers.h"
#include "opengl_gbuffer.h"
#include "opengl_post_processing_stack_resource.h"
#include "opengl_shadow_map.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"
#include <variant>

namespace tbx::plugins
{
    class OpenGlDrawResourceBundle final : public IOpenGlResource
    {
      public:
        explicit OpenGlDrawResourceBundle(OpenGlDrawResources draw_resources)
            : _draw_resources(std::move(draw_resources))
        {
        }

        void bind() override {}

        void unbind() override {}

        const OpenGlDrawResources& get_draw_resources() const
        {
            return _draw_resources;
        }

      private:
        OpenGlDrawResources _draw_resources = {};
    };

    static void hash_signature_bytes(uint64& hash_value, const void* data, const size_t size)
    {
        constexpr uint64 fnv_prime = 1099511628211ULL;
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t index = 0; index < size; ++index)
        {
            hash_value ^= static_cast<uint64>(bytes[index]);
            hash_value *= fnv_prime;
        }
    }

    template <typename TValue>
    static void hash_signature_value(uint64& hash_value, const TValue& value)
    {
        hash_signature_bytes(hash_value, &value, sizeof(value));
    }

    template <size_t N>
    static uint64 make_tagged_signature(const char (&tag)[N])
    {
        constexpr uint64 fnv_offset = 1469598103934665603ULL;
        auto hash_value = fnv_offset;
        hash_signature_bytes(hash_value, tag, N - 1U);
        return hash_value;
    }

    static uint64 hash_mesh_content_signature(const Mesh& mesh)
    {
        auto hash_value = make_tagged_signature("MeshContent");
        const auto vertex_count = static_cast<uint64>(mesh.vertices.size());
        const auto index_count = static_cast<uint64>(mesh.indices.size());
        hash_signature_value(hash_value, vertex_count);
        hash_signature_value(hash_value, index_count);

        if (!mesh.vertices.empty())
        {
            hash_signature_bytes(
                hash_value,
                mesh.vertices.data(),
                mesh.vertices.size() * sizeof(Vertex));
        }

        if (!mesh.indices.empty())
            hash_signature_bytes(
                hash_value,
                mesh.indices.data(),
                mesh.indices.size() * sizeof(uint32));

        return hash_value;
    }

    static uint64 hash_dynamic_mesh_pointer_signature(const std::shared_ptr<Mesh>& mesh)
    {
        auto hash_value = make_tagged_signature("DynamicMeshPointer");
        const auto pointer_value =
            static_cast<uint64>(reinterpret_cast<std::uintptr_t>(mesh.get()));
        hash_signature_value(hash_value, pointer_value);
        return hash_value;
    }

    static uint64 hash_static_mesh_signature(const Handle& model_handle)
    {
        auto hash_value = make_tagged_signature("StaticMesh");
        hash_signature_value(hash_value, model_handle.id.value);
        return hash_value;
    }

    static uint64 hash_shader_program_signature(const ShaderProgram& shader_program)
    {
        auto hash_value = make_tagged_signature("ShaderProgram");
        hash_signature_value(hash_value, shader_program.vertex.id.value);
        hash_signature_value(hash_value, shader_program.tesselation.id.value);
        hash_signature_value(hash_value, shader_program.geometry.id.value);
        hash_signature_value(hash_value, shader_program.fragment.id.value);
        hash_signature_value(hash_value, shader_program.compute.id.value);
        return hash_value;
    }

    static uint64 hash_shader_stage_signature(
        const Handle& shader_handle,
        const ShaderType stage_type)
    {
        auto hash_value = make_tagged_signature("ShaderStage");
        hash_signature_value(hash_value, shader_handle.id.value);
        hash_signature_value(hash_value, stage_type);
        return hash_value;
    }

    static void hash_texture_settings(uint64& hash_value, const TextureSettings& settings)
    {
        hash_signature_value(hash_value, settings.resolution.width);
        hash_signature_value(hash_value, settings.resolution.height);
        hash_signature_value(hash_value, settings.wrap);
        hash_signature_value(hash_value, settings.filter);
        hash_signature_value(hash_value, settings.format);
        hash_signature_value(hash_value, settings.mipmaps);
        hash_signature_value(hash_value, settings.compression);
    }

    static uint64 hash_texture_asset_signature(
        const Handle& texture_handle,
        const std::optional<TextureSettings>& override_settings)
    {
        auto hash_value = make_tagged_signature("TextureAsset");
        hash_signature_value(hash_value, texture_handle.id.value);
        const bool has_override = override_settings.has_value();
        hash_signature_value(hash_value, has_override);
        if (has_override)
            hash_texture_settings(hash_value, override_settings.value());
        return hash_value;
    }

    static uint64 hash_texture_content_signature(const Texture& texture_data)
    {
        auto hash_value = make_tagged_signature("TextureContent");
        hash_texture_settings(hash_value, texture_data);
        const auto pixel_count = static_cast<uint64>(texture_data.pixels.size());
        hash_signature_value(hash_value, pixel_count);
        if (!texture_data.pixels.empty())
        {
            hash_signature_bytes(
                hash_value,
                texture_data.pixels.data(),
                texture_data.pixels.size() * sizeof(Pixel));
        }

        return hash_value;
    }

    static std::string normalize_uniform_name(std::string_view name)
    {
        if (name.size() >= 2U && name[0] == 'u' && name[1] == '_')
            return std::string(name);

        std::string normalized = "u_";
        normalized.append(name.begin(), name.end());
        return normalized;
    }

    static void append_or_override_uniform(
        std::vector<MaterialParameter>& values,
        const MaterialParameter& uniform)
    {
        for (auto& existing : values)
        {
            if (existing.name != uniform.name)
                continue;

            existing = uniform;
            return;
        }

        values.push_back(uniform);
    }

    static void append_or_override_material_parameter(
        MaterialParameterBindings& parameters,
        std::string_view name,
        MaterialParameterData data)
    {
        parameters.set(name, std::move(data));
    }

    static void append_or_override_texture(
        MaterialTextureBindings& values,
        std::string_view name,
        const TextureInstance& runtime_texture)
    {
        values.set(name, runtime_texture);
    }

    static void append_or_override_runtime_texture(
        std::vector<MaterialTextureBinding>& values,
        std::string_view name,
        const TextureInstance& runtime_texture)
    {
        const auto normalized_name = normalize_uniform_name(name);
        for (auto& texture : values)
        {
            if (normalize_uniform_name(texture.name) != normalized_name)
                continue;

            texture.name = normalized_name;
            texture.texture = runtime_texture;
            return;
        }

        values.push_back(
            MaterialTextureBinding {
                .name = normalized_name,
                .texture = runtime_texture,
            });
    }

    static const TextureInstance* try_get_runtime_texture_override(
        const std::vector<MaterialTextureBinding>& values,
        const std::string& normalized_name)
    {
        for (const auto& override_texture : values)
        {
            if (normalize_uniform_name(override_texture.name) != normalized_name)
                continue;

            return &override_texture.texture;
        }

        return nullptr;
    }

    static uint64 hash_runtime_material(const MaterialInstance& runtime_material)
    {
        auto hash_value = make_tagged_signature("MaterialHandle");
        hash_signature_value(hash_value, runtime_material.handle.id.value);
        return hash_value;
    }

    static uint64 hash_runtime_material_resource_identity(const MaterialInstance& runtime_material)
    {
        return hash_runtime_material(runtime_material);
    }

    static uint64 hash_post_process_stack(const PostProcessing& post_processing)
    {
        constexpr uint64 fnv_offset = 1469598103934665603ULL;
        constexpr uint64 fnv_prime = 1099511628211ULL;
        auto hash_value = fnv_offset;

        auto hash_bytes = [&hash_value](const void* data, size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t index = 0; index < size; ++index)
            {
                hash_value ^= static_cast<uint64>(bytes[index]);
                hash_value *= fnv_prime;
            }
        };

        for (const auto& effect : post_processing.effects)
        {
            const auto material_hash = hash_runtime_material(effect.material);
            hash_bytes(&material_hash, sizeof(material_hash));
            hash_bytes(&effect.is_enabled, sizeof(effect.is_enabled));
            hash_bytes(&effect.blend, sizeof(effect.blend));
        }

        hash_bytes(&post_processing.is_enabled, sizeof(post_processing.is_enabled));
        return hash_value;
    }

    static void apply_runtime_material_overrides(
        const MaterialInstance& runtime_material,
        Material& in_out_material,
        std::vector<MaterialTextureBinding>* out_runtime_texture_overrides)
    {
        for (const auto& texture_binding : runtime_material.textures)
        {
            append_or_override_texture(
                in_out_material.textures,
                texture_binding.name,
                texture_binding.texture);
            if (out_runtime_texture_overrides != nullptr)
                append_or_override_runtime_texture(
                    *out_runtime_texture_overrides,
                    texture_binding.name,
                    texture_binding.texture);
        }

        for (const auto& parameter_override : runtime_material.parameters)
        {
            append_or_override_material_parameter(
                in_out_material.parameters,
                parameter_override.name,
                parameter_override.value);
        }
    }

    static void apply_default_material_parameters(MaterialParameterBindings& parameters)
    {
        if (!parameters.has("color"))
            parameters.set("color", Color::WHITE);
        if (!parameters.has("emissive"))
            parameters.set("emissive", Color::BLACK);
        if (!parameters.has("unlit"))
            parameters.set("unlit", false);
        if (!parameters.has("roughness"))
            parameters.set("roughness", 1.0F);
        if (!parameters.has("specular"))
            parameters.set("specular", 1.0F);
        if (!parameters.has("occlusion"))
            parameters.set("occlusion", 1.0F);
        if (!parameters.has("alpha_cutoff"))
            parameters.set("alpha_cutoff", 0.1F);
    }

    static void append_texture_binding(
        OpenGlDrawResources& out_resources,
        std::string_view uniform_name,
        const std::shared_ptr<OpenGlTexture>& texture)
    {
        auto texture_slot = static_cast<int>(out_resources.textures.size());
        out_resources.textures.push_back(
            OpenGlTextureBinding {
                .uniform_name = std::string(uniform_name),
                .texture = texture,
                .slot = texture_slot,
            });
    }

    static Material resolve_static_mesh_material(const Model& model)
    {
        Material resolved = Material();

        for (const auto& part : model.parts)
        {
            if (part.mesh_index != 0U)
                continue;

            const auto material_index = static_cast<size_t>(part.material_index);
            if (material_index < model.materials.size())
                return model.materials[material_index];

            break;
        }

        if (!model.materials.empty())
            resolved = model.materials[0];

        return resolved;
    }

    OpenGlResourceManager::OpenGlResourceManager(AssetManager& asset_manager)
        : _asset_manager(asset_manager)
    {
    }

    bool OpenGlResourceManager::try_get(
        const Entity& entity,
        OpenGlDrawResources& out_resources,
        const bool pin)
    {
        if (entity.has_component<Sky>())
        {
            const auto& sky = entity.get_component<Sky>();
            const uint64 material_hash = hash_runtime_material(sky.material);
            const auto entity_id = entity.get_id();
            auto iterator = _draw_resources_by_entity.find(entity_id);
            if (iterator != _draw_resources_by_entity.end()
                && iterator->second.signature == material_hash
                && try_get_cached_draw_resources(iterator->second, out_resources))
            {
                iterator->second.last_use = Clock::now();
                iterator->second.is_pinned = iterator->second.is_pinned || pin;
                return true;
            }

            auto resources = OpenGlDrawResources {};
            auto cache_entry = OpenGlCachedDrawResourceEntry {};
            if (!try_create_sky_resources(sky.material, resources, &cache_entry))
                return false;

            _draw_resources_by_entity[entity_id] = OpenGlCachedDrawResourceEntry {
                .mesh_signature = cache_entry.mesh_signature,
                .shader_program_signature = cache_entry.shader_program_signature,
                .textures = std::move(cache_entry.textures),
                .shader_parameters = std::move(cache_entry.shader_parameters),
                .use_tesselation = cache_entry.use_tesselation,
                .last_use = Clock::now(),
                .signature = material_hash,
                .is_pinned = pin,
            };
            out_resources = std::move(resources);
            return true;
        }

        if (entity.has_component<DynamicMesh>())
        {
            const auto& renderer = entity.get_component<Renderer>();
            const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
            const uint64 material_signature =
                hash_runtime_material_resource_identity(renderer.material);
            const uint64 mesh_signature =
                static_cast<uint64>(reinterpret_cast<std::uintptr_t>(dynamic_mesh.data.get()));

            const auto resource_id = entity.get_id();
            auto iterator = _draw_resources_by_entity.find(resource_id);
            if (iterator != _draw_resources_by_entity.end()
                && iterator->second.signature == material_signature
                && iterator->second.aux_signature == mesh_signature
                && try_get_cached_draw_resources(iterator->second, out_resources))
            {
                iterator->second.last_use = Clock::now();
                iterator->second.is_pinned = iterator->second.is_pinned || pin;
                return true;
            }

            if (iterator != _draw_resources_by_entity.end()
                && iterator->second.signature == material_signature && dynamic_mesh.data)
            {
                auto resources = OpenGlDrawResources {};
                if (try_get_cached_draw_resources(iterator->second, resources))
                {
                    const auto shared_mesh_signature =
                        hash_dynamic_mesh_pointer_signature(dynamic_mesh.data);
                    resources.mesh =
                        get_or_create_shared_mesh(shared_mesh_signature, *dynamic_mesh.data);
                    iterator->second.mesh_signature = shared_mesh_signature;
                    iterator->second.aux_signature = mesh_signature;
                    iterator->second.last_use = Clock::now();
                    iterator->second.is_pinned = iterator->second.is_pinned || pin;
                    out_resources = std::move(resources);
                    return true;
                }
            }

            auto resources = OpenGlDrawResources {};
            auto cache_entry = OpenGlCachedDrawResourceEntry {};
            if (!try_create_dynamic_mesh_resources(entity, renderer, resources, &cache_entry))
                return false;

            _draw_resources_by_entity[resource_id] = OpenGlCachedDrawResourceEntry {
                .mesh_signature = cache_entry.mesh_signature,
                .shader_program_signature = cache_entry.shader_program_signature,
                .textures = std::move(cache_entry.textures),
                .shader_parameters = std::move(cache_entry.shader_parameters),
                .use_tesselation = cache_entry.use_tesselation,
                .last_use = Clock::now(),
                .signature = material_signature,
                .aux_signature = mesh_signature,
                .is_pinned = pin,
            };
            out_resources = std::move(resources);
            return true;
        }

        if (!entity.has_component<StaticMesh>())
            return false;

        const auto& renderer = entity.get_component<Renderer>();
        const auto& static_mesh = entity.get_component<StaticMesh>();
        const uint64 material_signature =
            hash_runtime_material_resource_identity(renderer.material);
        const uint64 model_signature = static_cast<uint64>(static_mesh.handle.id.value);
        const auto resource_id = entity.get_id();
        auto iterator = _draw_resources_by_entity.find(resource_id);
        if (iterator != _draw_resources_by_entity.end()
            && iterator->second.signature == material_signature
            && iterator->second.aux_signature == model_signature
            && try_get_cached_draw_resources(iterator->second, out_resources))
        {
            iterator->second.last_use = Clock::now();
            iterator->second.is_pinned = iterator->second.is_pinned || pin;
            return true;
        }

        auto resources = OpenGlDrawResources {};
        auto cache_entry = OpenGlCachedDrawResourceEntry {};
        if (!try_create_static_mesh_resources(entity, renderer, resources, &cache_entry))
            return false;

        _draw_resources_by_entity[resource_id] = OpenGlCachedDrawResourceEntry {
            .mesh_signature = cache_entry.mesh_signature,
            .shader_program_signature = cache_entry.shader_program_signature,
            .textures = std::move(cache_entry.textures),
            .shader_parameters = std::move(cache_entry.shader_parameters),
            .use_tesselation = cache_entry.use_tesselation,
            .last_use = Clock::now(),
            .signature = material_signature,
            .aux_signature = model_signature,
            .is_pinned = pin,
        };
        out_resources = std::move(resources);
        return true;
    }

    bool OpenGlResourceManager::add(const Entity& entity, const bool pin)
    {
        if (entity.has_component<PostProcessing>())
        {
            const auto resource_id = entity.get_id();
            if (!resource_id.is_valid())
                return false;

            const auto& post_processing = entity.get_component<PostProcessing>();
            const auto post_process_signature = hash_post_process_stack(post_processing);
            auto iterator = _resources_by_entity.find(resource_id);
            if (iterator != _resources_by_entity.end()
                && iterator->second.signature == post_process_signature
                && std::dynamic_pointer_cast<OpenGlPostProcessingStackResource>(
                    iterator->second.resource))
            {
                iterator->second.last_use = Clock::now();
                iterator->second.is_pinned = iterator->second.is_pinned || pin;
                return true;
            }

            auto stack_resource = std::shared_ptr<IOpenGlResource> {};
            auto stack_signature = uint64 {};
            if (!try_create_post_processing_stack_resource(
                    post_processing,
                    stack_resource,
                    stack_signature))
            {
                return false;
            }

            _resources_by_entity[resource_id] = OpenGlCachedResourceEntry {
                .resource = stack_resource,
                .last_use = Clock::now(),
                .signature = stack_signature,
                .is_pinned = pin,
            };
            return true;
        }

        auto out_draw_resources = OpenGlDrawResources {};
        return try_get(entity, out_draw_resources, pin);
    }

    void OpenGlResourceManager::append_signature_bytes(
        uint64& hash_value,
        const void* data,
        const size_t size)
    {
        constexpr uint64 fnv_prime = 1099511628211ULL;
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t index = 0; index < size; ++index)
        {
            hash_value ^= static_cast<uint64>(bytes[index]);
            hash_value *= fnv_prime;
        }
    }

    bool OpenGlResourceManager::try_get(
        const Uuid& resource_uuid,
        std::shared_ptr<IOpenGlResource>& out_resource) const
    {
        return try_get_stored_resource_base(resource_uuid, out_resource);
    }

    bool OpenGlResourceManager::try_get(
        const Entity& entity,
        std::shared_ptr<IOpenGlResource>& out_resource,
        const bool pin)
    {
        out_resource = nullptr;
        const auto resource_id = entity.get_id();
        if (!resource_id.is_valid())
            return false;

        auto entity_iterator = _resources_by_entity.find(resource_id);
        if (entity_iterator != _resources_by_entity.end() && entity_iterator->second.resource)
        {
            entity_iterator->second.last_use = Clock::now();
            entity_iterator->second.is_pinned = entity_iterator->second.is_pinned || pin;
            out_resource = entity_iterator->second.resource;
            return true;
        }

        auto draw_iterator = _draw_resources_by_entity.find(resource_id);
        if (draw_iterator != _draw_resources_by_entity.end())
        {
            auto draw_resources = OpenGlDrawResources {};
            if (try_get_cached_draw_resources(draw_iterator->second, draw_resources))
            {
                draw_iterator->second.last_use = Clock::now();
                draw_iterator->second.is_pinned = draw_iterator->second.is_pinned || pin;
                out_resource = std::make_shared<OpenGlDrawResourceBundle>(draw_resources);
                return true;
            }
        }

        if (!add(entity, pin))
            return false;

        entity_iterator = _resources_by_entity.find(resource_id);
        if (entity_iterator != _resources_by_entity.end() && entity_iterator->second.resource)
        {
            out_resource = entity_iterator->second.resource;
            return true;
        }

        draw_iterator = _draw_resources_by_entity.find(resource_id);
        if (draw_iterator != _draw_resources_by_entity.end())
        {
            auto draw_resources = OpenGlDrawResources {};
            if (!try_get_cached_draw_resources(draw_iterator->second, draw_resources))
                return false;

            draw_iterator->second.last_use = Clock::now();
            draw_iterator->second.is_pinned = draw_iterator->second.is_pinned || pin;
            out_resource = std::make_shared<OpenGlDrawResourceBundle>(draw_resources);
            return true;
        }

        return false;
    }

    bool OpenGlResourceManager::try_get_or_create_shader_program(
        const ShaderProgram& shader_program,
        std::shared_ptr<OpenGlShaderProgram>& out_program)
    {
        out_program = get_or_create_shared_shader_program(shader_program, nullptr);
        return out_program != nullptr;
    }

    std::shared_ptr<OpenGlMesh> OpenGlResourceManager::get_or_create_runtime_mesh(
        const uint64 mesh_signature,
        const Mesh& mesh)
    {
        return get_or_create_shared_mesh(mesh_signature, mesh);
    }

    bool OpenGlResourceManager::try_get_stored_resource_base(
        const Uuid& resource_uuid,
        std::shared_ptr<IOpenGlResource>& out_resource) const
    {
        out_resource = nullptr;
        if (!resource_uuid.is_valid())
            return false;

        auto iterator = _resources_by_entity.find(resource_uuid);
        if (iterator == _resources_by_entity.end())
            return false;

        out_resource = iterator->second.resource;
        return out_resource != nullptr;
    }

    bool OpenGlResourceManager::try_get_cached_draw_resources(
        const OpenGlCachedDrawResourceEntry& cached_entry,
        OpenGlDrawResources& out_resources)
    {
        out_resources = {};
        if (cached_entry.mesh_signature == 0U)
            return false;

        auto mesh_iterator = _shared_meshes_by_signature.find(cached_entry.mesh_signature);
        if (mesh_iterator == _shared_meshes_by_signature.end())
            return false;
        out_resources.mesh = mesh_iterator->second.resource;
        if (!out_resources.mesh)
            return false;
        mesh_iterator->second.last_use = Clock::now();

        if (cached_entry.shader_program_signature != 0U)
        {
            auto program_iterator =
                _shared_shader_programs_by_signature.find(cached_entry.shader_program_signature);
            if (program_iterator == _shared_shader_programs_by_signature.end())
                return false;
            out_resources.shader_program = program_iterator->second.resource;
            if (!out_resources.shader_program)
                return false;
            program_iterator->second.last_use = Clock::now();
        }

        out_resources.textures.reserve(cached_entry.textures.size());
        for (const auto& texture_reference : cached_entry.textures)
        {
            auto texture_iterator =
                _shared_textures_by_signature.find(texture_reference.texture_signature);
            if (texture_iterator == _shared_textures_by_signature.end())
                return false;

            auto texture = texture_iterator->second.resource;
            if (!texture)
                return false;
            texture_iterator->second.last_use = Clock::now();

            out_resources.textures.push_back(
                OpenGlTextureBinding {
                    .uniform_name = texture_reference.uniform_name,
                    .texture = texture,
                    .slot = texture_reference.slot,
                });
        }

        out_resources.shader_parameters = cached_entry.shader_parameters;
        out_resources.use_tesselation = cached_entry.use_tesselation;
        return true;
    }

    std::shared_ptr<OpenGlMesh> OpenGlResourceManager::get_or_create_shared_mesh(
        const uint64 mesh_signature,
        const Mesh& mesh)
    {
        auto iterator = _shared_meshes_by_signature.find(mesh_signature);
        if (iterator != _shared_meshes_by_signature.end())
        {
            auto cached_mesh = iterator->second.resource;
            if (cached_mesh)
            {
                iterator->second.last_use = Clock::now();
                return cached_mesh;
            }
        }

        auto shared_mesh = std::make_shared<OpenGlMesh>(mesh);
        _shared_meshes_by_signature[mesh_signature] = OpenGlSharedResourceCacheEntry<OpenGlMesh> {
            .resource = shared_mesh,
            .last_use = Clock::now(),
        };
        return shared_mesh;
    }

    std::shared_ptr<OpenGlShader> OpenGlResourceManager::get_or_create_shared_shader_stage(
        const Handle& shader_handle,
        const ShaderType expected_type)
    {
        if (!shader_handle.is_valid())
            return nullptr;

        const auto stage_signature = hash_shader_stage_signature(shader_handle, expected_type);
        auto iterator = _shared_shader_stages_by_signature.find(stage_signature);
        if (iterator != _shared_shader_stages_by_signature.end())
        {
            auto cached_stage = iterator->second.resource;
            if (cached_stage)
            {
                iterator->second.last_use = Clock::now();
                return cached_stage;
            }
        }

        auto shader_asset = _asset_manager.get().load<Shader>(shader_handle);
        TBX_ASSERT(
            shader_asset != nullptr,
            "OpenGL rendering: material shader stage could not be loaded.");
        if (!shader_asset)
            return nullptr;

        const auto* stage_source = static_cast<const ShaderSource*>(nullptr);
        for (const auto& source : shader_asset->sources)
        {
            if (source.type != expected_type)
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: shader source type does not match expected stage type.");
                continue;
            }

            stage_source = &source;
            break;
        }

        TBX_ASSERT(
            stage_source != nullptr,
            "OpenGL rendering: material shader stage is missing expected source type.");
        if (!stage_source)
            return nullptr;

        auto stage = std::make_shared<OpenGlShader>(*stage_source);
        TBX_ASSERT(
            stage->get_shader_id() != 0,
            "OpenGL rendering: material shader stage failed to compile.");
        if (stage->get_shader_id() == 0)
            return nullptr;

        _shared_shader_stages_by_signature[stage_signature] =
            OpenGlSharedResourceCacheEntry<OpenGlShader> {
                .resource = stage,
                .last_use = Clock::now(),
            };
        return stage;
    }

    std::shared_ptr<OpenGlShaderProgram> OpenGlResourceManager::get_or_create_shared_shader_program(
        const ShaderProgram& shader_program,
        uint64* out_program_signature)
    {
        if (out_program_signature != nullptr)
            *out_program_signature = 0U;

        if (!shader_program.is_valid())
            return nullptr;

        const auto program_signature = hash_shader_program_signature(shader_program);

        auto iterator = _shared_shader_programs_by_signature.find(program_signature);
        if (iterator != _shared_shader_programs_by_signature.end())
        {
            auto cached_program = iterator->second.resource;
            if (cached_program)
            {
                iterator->second.last_use = Clock::now();
                if (out_program_signature != nullptr)
                    *out_program_signature = program_signature;
                return cached_program;
            }
        }

        auto shader_stages = std::vector<std::shared_ptr<OpenGlShader>> {};
        shader_stages.reserve(5U);
        const auto append_shader_stage =
            [this, &shader_stages](const Handle& shader_handle, const ShaderType expected_type)
        {
            auto stage = get_or_create_shared_shader_stage(shader_handle, expected_type);
            if (!stage)
                return;

            shader_stages.push_back(std::move(stage));
        };

        append_shader_stage(shader_program.vertex, ShaderType::VERTEX);
        append_shader_stage(shader_program.tesselation, ShaderType::TESSELATION);
        append_shader_stage(shader_program.geometry, ShaderType::GEOMETRY);
        append_shader_stage(shader_program.fragment, ShaderType::FRAGMENT);
        append_shader_stage(shader_program.compute, ShaderType::COMPUTE);
        if (shader_stages.empty())
            return nullptr;

        auto linked_program = std::make_shared<OpenGlShaderProgram>(shader_stages);
        TBX_ASSERT(
            linked_program->get_program_id() != 0,
            "OpenGL rendering: failed to link a valid shader program.");
        if (linked_program->get_program_id() == 0)
            return nullptr;

        _shared_shader_programs_by_signature[program_signature] =
            OpenGlSharedResourceCacheEntry<OpenGlShaderProgram> {
                .resource = linked_program,
                .last_use = Clock::now(),
            };
        if (out_program_signature != nullptr)
            *out_program_signature = program_signature;
        return linked_program;
    }

    std::shared_ptr<OpenGlTexture> OpenGlResourceManager::get_or_create_shared_texture(
        const uint64 texture_signature,
        const Texture& texture_data)
    {
        auto iterator = _shared_textures_by_signature.find(texture_signature);
        if (iterator != _shared_textures_by_signature.end())
        {
            auto cached_texture = iterator->second.resource;
            if (cached_texture)
            {
                iterator->second.last_use = Clock::now();
                return cached_texture;
            }
        }

        auto texture = std::make_shared<OpenGlTexture>(texture_data);
        _shared_textures_by_signature[texture_signature] =
            OpenGlSharedResourceCacheEntry<OpenGlTexture> {
                .resource = texture,
                .last_use = Clock::now(),
            };
        return texture;
    }

    void OpenGlResourceManager::pin(const Uuid& resource_uuid)
    {
        if (!resource_uuid.is_valid())
            return;

        auto draw_iterator = _draw_resources_by_entity.find(resource_uuid);
        if (draw_iterator != _draw_resources_by_entity.end())
            draw_iterator->second.is_pinned = true;

        auto entity_iterator = _resources_by_entity.find(resource_uuid);
        if (entity_iterator != _resources_by_entity.end())
            entity_iterator->second.is_pinned = true;
    }

    void OpenGlResourceManager::unpin(const Uuid& resource_uuid)
    {
        if (!resource_uuid.is_valid())
            return;

        auto draw_iterator = _draw_resources_by_entity.find(resource_uuid);
        if (draw_iterator != _draw_resources_by_entity.end())
            draw_iterator->second.is_pinned = false;

        auto entity_iterator = _resources_by_entity.find(resource_uuid);
        if (entity_iterator != _resources_by_entity.end())
            entity_iterator->second.is_pinned = false;
    }

    void OpenGlResourceManager::clear()
    {
        _draw_resources_by_entity.clear();
        _resources_by_entity.clear();
        _shared_meshes_by_signature.clear();
        _shared_shader_stages_by_signature.clear();
        _shared_shader_programs_by_signature.clear();
        _shared_textures_by_signature.clear();
        _next_unused_scan_time = {};
    }

    void OpenGlResourceManager::clear_unused()
    {
        const auto now = Clock::now();
        if (_next_unused_scan_time > now)
            return;

        _next_unused_scan_time = now + UNUSED_SCAN_INTERVAL;
        const auto unload_before = Clock::now() - UNUSED_TTL;

        for (auto iterator = _draw_resources_by_entity.begin();
             iterator != _draw_resources_by_entity.end();)
        {
            if (iterator->second.is_pinned)
            {
                ++iterator;
                continue;
            }
            if (iterator->second.last_use >= unload_before)
            {
                ++iterator;
                continue;
            }
            iterator = _draw_resources_by_entity.erase(iterator);
        }

        for (auto iterator = _resources_by_entity.begin(); iterator != _resources_by_entity.end();)
        {
            if (iterator->second.is_pinned)
            {
                ++iterator;
                continue;
            }
            if (iterator->second.last_use >= unload_before)
            {
                ++iterator;
                continue;
            }
            iterator = _resources_by_entity.erase(iterator);
        }

        for (auto iterator = _shared_meshes_by_signature.begin();
             iterator != _shared_meshes_by_signature.end();)
        {
            if (!iterator->second.resource)
            {
                iterator = _shared_meshes_by_signature.erase(iterator);
                continue;
            }
            if (iterator->second.last_use >= unload_before
                || iterator->second.resource.use_count() > 1)
            {
                ++iterator;
                continue;
            }

            iterator = _shared_meshes_by_signature.erase(iterator);
        }

        for (auto iterator = _shared_shader_stages_by_signature.begin();
             iterator != _shared_shader_stages_by_signature.end();)
        {
            if (!iterator->second.resource)
            {
                iterator = _shared_shader_stages_by_signature.erase(iterator);
                continue;
            }
            if (iterator->second.last_use >= unload_before
                || iterator->second.resource.use_count() > 1)
            {
                ++iterator;
                continue;
            }

            iterator = _shared_shader_stages_by_signature.erase(iterator);
        }

        for (auto iterator = _shared_shader_programs_by_signature.begin();
             iterator != _shared_shader_programs_by_signature.end();)
        {
            if (!iterator->second.resource)
            {
                iterator = _shared_shader_programs_by_signature.erase(iterator);
                continue;
            }
            if (iterator->second.last_use >= unload_before
                || iterator->second.resource.use_count() > 1)
            {
                ++iterator;
                continue;
            }

            iterator = _shared_shader_programs_by_signature.erase(iterator);
        }

        for (auto iterator = _shared_textures_by_signature.begin();
             iterator != _shared_textures_by_signature.end();)
        {
            if (!iterator->second.resource)
            {
                iterator = _shared_textures_by_signature.erase(iterator);
                continue;
            }
            if (iterator->second.last_use >= unload_before
                || iterator->second.resource.use_count() > 1)
            {
                ++iterator;
                continue;
            }

            iterator = _shared_textures_by_signature.erase(iterator);
        }
    }

    bool OpenGlResourceManager::try_create_static_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources,
        OpenGlCachedDrawResourceEntry* out_cache_entry)
    {
        const auto& static_mesh = entity.get_component<StaticMesh>();
        auto model = _asset_manager.get().load<Model>(static_mesh.handle);
        if (!model || model->meshes.empty())
            return false;

        constexpr uint32 static_mesh_index = 0U;
        const auto static_mesh_signature = hash_static_mesh_signature(static_mesh.handle);
        out_resources.mesh =
            get_or_create_shared_mesh(static_mesh_signature, model->meshes[static_mesh_index]);
        if (out_cache_entry != nullptr)
            out_cache_entry->mesh_signature = static_mesh_signature;

        auto material = Material();
        if (renderer.material.handle.is_valid())
        {
            auto override_material = _asset_manager.get().load<Material>(renderer.material.handle);
            if (override_material)
                material = *override_material;
        }
        else
        {
            material = resolve_static_mesh_material(*model);
        }

        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(renderer.material, material, &runtime_texture_overrides);
        auto texture_references = std::vector<OpenGlTextureResourceReference> {};
        auto shader_program_signature = uint64 {};
        const auto did_append_material = try_append_material_resources(
            material,
            runtime_texture_overrides,
            out_resources,
            &shader_program_signature,
            &texture_references,
            true);
        if (did_append_material && out_cache_entry != nullptr)
        {
            out_cache_entry->shader_program_signature = shader_program_signature;
            out_cache_entry->textures = std::move(texture_references);
            out_cache_entry->shader_parameters = out_resources.shader_parameters;
            out_cache_entry->use_tesselation = out_resources.use_tesselation;
        }

        return did_append_material;
    }

    bool OpenGlResourceManager::try_create_dynamic_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources,
        OpenGlCachedDrawResourceEntry* out_cache_entry)
    {
        const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
        if (!dynamic_mesh.data)
            return false;

        const auto dynamic_mesh_signature = hash_dynamic_mesh_pointer_signature(dynamic_mesh.data);
        out_resources.mesh = get_or_create_shared_mesh(dynamic_mesh_signature, *dynamic_mesh.data);
        if (out_cache_entry != nullptr)
            out_cache_entry->mesh_signature = dynamic_mesh_signature;

        auto material = Material();
        if (renderer.material.handle.is_valid())
        {
            auto loaded_material = _asset_manager.get().load<Material>(renderer.material.handle);
            if (loaded_material)
                material = *loaded_material;
        }

        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(renderer.material, material, &runtime_texture_overrides);
        auto texture_references = std::vector<OpenGlTextureResourceReference> {};
        auto shader_program_signature = uint64 {};
        const auto did_append_material = try_append_material_resources(
            material,
            runtime_texture_overrides,
            out_resources,
            &shader_program_signature,
            &texture_references,
            true);
        if (did_append_material && out_cache_entry != nullptr)
        {
            out_cache_entry->shader_program_signature = shader_program_signature;
            out_cache_entry->textures = std::move(texture_references);
            out_cache_entry->shader_parameters = out_resources.shader_parameters;
            out_cache_entry->use_tesselation = out_resources.use_tesselation;
        }

        return did_append_material;
    }

    bool OpenGlResourceManager::try_create_post_processing_stack_resource(
        const PostProcessing& post_processing,
        std::shared_ptr<IOpenGlResource>& out_resource,
        uint64& out_signature)
    {
        out_resource = nullptr;
        out_signature = hash_post_process_stack(post_processing);

        auto stack_resource = std::make_shared<OpenGlPostProcessingStackResource>();
        for (const auto& effect : post_processing.effects)
        {
            if (!effect.is_enabled)
                continue;
            if (!effect.material.handle.is_valid())
                continue;

            auto draw_resources = OpenGlDrawResources {};
            if (!try_create_post_process_resources(effect.material, draw_resources, nullptr))
                continue;
            if (!draw_resources.mesh || !draw_resources.shader_program)
                continue;

            stack_resource->add_effect(
                OpenGlPostProcessingStackResource::Effect {
                    .draw_resources = draw_resources,
                    .blend = effect.blend,
                });
        }

        out_resource = stack_resource;
        return true;
    }

    bool OpenGlResourceManager::try_create_sky_resources(
        const MaterialInstance& material,
        OpenGlDrawResources& out_resources,
        OpenGlCachedDrawResourceEntry* out_cache_entry)
    {
        if (!material.handle.is_valid())
            return false;
        auto source_material = _asset_manager.get().load<Material>(material.handle);
        if (!source_material)
            return false;

        static const uint64 sky_dome_signature = hash_mesh_content_signature(sky_dome);
        out_resources.mesh = get_or_create_shared_mesh(sky_dome_signature, sky_dome);
        if (out_cache_entry != nullptr)
            out_cache_entry->mesh_signature = sky_dome_signature;

        auto resolved = *source_material;
        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(material, resolved, &runtime_texture_overrides);
        auto texture_references = std::vector<OpenGlTextureResourceReference> {};
        auto shader_program_signature = uint64 {};
        const auto did_append_material = try_append_material_resources(
            resolved,
            runtime_texture_overrides,
            out_resources,
            &shader_program_signature,
            &texture_references,
            false);
        if (did_append_material && out_cache_entry != nullptr)
        {
            out_cache_entry->shader_program_signature = shader_program_signature;
            out_cache_entry->textures = std::move(texture_references);
            out_cache_entry->shader_parameters = out_resources.shader_parameters;
            out_cache_entry->use_tesselation = out_resources.use_tesselation;
        }

        return did_append_material;
    }

    bool OpenGlResourceManager::try_create_post_process_resources(
        const MaterialInstance& material,
        OpenGlDrawResources& out_resources,
        OpenGlCachedDrawResourceEntry* out_cache_entry)
    {
        if (!material.handle.is_valid())
            return false;
        auto source_material = _asset_manager.get().load<Material>(material.handle);
        if (!source_material)
            return false;

        static const uint64 fullscreen_quad_signature =
            hash_mesh_content_signature(fullscreen_quad);
        out_resources.mesh = get_or_create_shared_mesh(fullscreen_quad_signature, fullscreen_quad);
        if (out_cache_entry != nullptr)
            out_cache_entry->mesh_signature = fullscreen_quad_signature;

        auto resolved = *source_material;
        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(material, resolved, &runtime_texture_overrides);
        auto texture_references = std::vector<OpenGlTextureResourceReference> {};
        auto shader_program_signature = uint64 {};
        const auto did_append_material = try_append_material_resources(
            resolved,
            runtime_texture_overrides,
            out_resources,
            &shader_program_signature,
            &texture_references,
            false);
        if (did_append_material && out_cache_entry != nullptr)
        {
            out_cache_entry->shader_program_signature = shader_program_signature;
            out_cache_entry->textures = std::move(texture_references);
            out_cache_entry->shader_parameters = out_resources.shader_parameters;
            out_cache_entry->use_tesselation = out_resources.use_tesselation;
        }

        return did_append_material;
    }

    bool OpenGlResourceManager::try_append_material_resources(
        const Material& material,
        const std::vector<MaterialTextureBinding>& runtime_texture_overrides,
        OpenGlDrawResources& out_resources,
        uint64* out_shader_program_signature,
        std::vector<OpenGlTextureResourceReference>* out_texture_references,
        const bool force_deferred_geometry_program)
    {
        if (out_shader_program_signature != nullptr)
            *out_shader_program_signature = 0U;
        if (out_texture_references != nullptr)
            out_texture_references->clear();

        auto resolved_material = material;
        if (force_deferred_geometry_program)
        {
            resolved_material.program.vertex = deferred_geometry_vertex_shader;
            resolved_material.program.fragment = deferred_geometry_fragment_shader;
            resolved_material.program.tesselation = {};
            resolved_material.program.geometry = {};
            resolved_material.program.compute = {};
        }
        else if (!resolved_material.program.is_valid())
        {
            resolved_material.program.vertex = lit_vertex_shader;
            resolved_material.program.fragment = lit_fragment_shader;
        }
        apply_default_material_parameters(resolved_material.parameters);

        if (resolved_material.program.is_valid())
        {
            out_resources.use_tesselation = resolved_material.program.tesselation.is_valid();
            out_resources.shader_program = get_or_create_shared_shader_program(
                resolved_material.program,
                out_shader_program_signature);
        }

        const bool has_diffuse = resolved_material.textures.get("diffuse") != nullptr;
        const bool has_normal = resolved_material.textures.get("normal") != nullptr;
        for (const auto& texture_binding : resolved_material.textures)
        {
            const auto normalized_name = normalize_uniform_name(texture_binding.name);

            const TextureInstance* runtime_texture_override =
                try_get_runtime_texture_override(runtime_texture_overrides, normalized_name);

            std::shared_ptr<Texture> texture_asset = nullptr;
            Handle resolved_texture_handle = texture_binding.texture.handle;
            if (runtime_texture_override && runtime_texture_override->handle.is_valid())
                resolved_texture_handle = runtime_texture_override->handle;
            if (resolved_texture_handle.is_valid())
            {
                auto load_parameters = TextureLoadParameters {};
                if (runtime_texture_override && runtime_texture_override->settings.has_value())
                    load_parameters.settings = runtime_texture_override->settings.value();

                texture_asset = _asset_manager.get().load(resolved_texture_handle, load_parameters);
                TBX_ASSERT(
                    texture_asset != nullptr,
                    "OpenGL rendering: texture '{}' failed to load as Texture (id={}, name='{}').",
                    normalized_name,
                    to_string(resolved_texture_handle.id),
                    resolved_texture_handle.name);
            }

            auto fallback_texture = Texture();
            if (normalized_name == "u_normal")
            {
                fallback_texture.format = TextureFormat::RGB;
                fallback_texture.pixels = {128, 128, 255};
            }

            auto texture_data = texture_asset ? *texture_asset : fallback_texture;
            auto texture_signature = uint64 {};
            if (runtime_texture_override && runtime_texture_override->settings.has_value())
            {
                const TextureSettings& texture_settings =
                    runtime_texture_override->settings.value();
                texture_data.filter = texture_settings.filter;
                texture_data.wrap = texture_settings.wrap;
                texture_data.format = texture_settings.format;
                texture_data.mipmaps = texture_settings.mipmaps;
                texture_data.compression = texture_settings.compression;
            }

            if (texture_asset && resolved_texture_handle.is_valid())
            {
                auto override_settings = std::optional<TextureSettings> {};
                if (runtime_texture_override && runtime_texture_override->settings.has_value())
                    override_settings = runtime_texture_override->settings.value();
                texture_signature =
                    hash_texture_asset_signature(resolved_texture_handle, override_settings);
            }
            else
            {
                texture_signature = hash_texture_content_signature(texture_data);
            }

            auto texture = get_or_create_shared_texture(texture_signature, texture_data);
            append_texture_binding(out_resources, normalized_name, texture);
            if (out_texture_references != nullptr)
            {
                const int slot = static_cast<int>(out_resources.textures.size()) - 1;
                out_texture_references->push_back(
                    OpenGlTextureResourceReference {
                        .uniform_name = std::string(normalized_name),
                        .texture_signature = texture_signature,
                        .slot = slot,
                    });
            }
        }

        if (!has_diffuse)
        {
            auto fallback_texture = Texture();
            const auto texture_signature = hash_texture_content_signature(fallback_texture);
            auto texture = get_or_create_shared_texture(texture_signature, fallback_texture);
            append_texture_binding(out_resources, "u_diffuse", texture);
            if (out_texture_references != nullptr)
            {
                const int slot = static_cast<int>(out_resources.textures.size()) - 1;
                out_texture_references->push_back(
                    OpenGlTextureResourceReference {
                        .uniform_name = "u_diffuse",
                        .texture_signature = texture_signature,
                        .slot = slot,
                    });
            }
        }
        if (!has_normal)
        {
            auto normal_texture = Texture();
            normal_texture.format = TextureFormat::RGB;
            normal_texture.pixels = {128, 128, 255};
            const auto texture_signature = hash_texture_content_signature(normal_texture);
            auto texture = get_or_create_shared_texture(texture_signature, normal_texture);
            append_texture_binding(out_resources, "u_normal", texture);
            if (out_texture_references != nullptr)
            {
                const int slot = static_cast<int>(out_resources.textures.size()) - 1;
                out_texture_references->push_back(
                    OpenGlTextureResourceReference {
                        .uniform_name = "u_normal",
                        .texture_signature = texture_signature,
                        .slot = slot,
                    });
            }
        }

        for (const auto& parameter_binding : resolved_material.parameters)
        {
            append_or_override_uniform(
                out_resources.shader_parameters,
                MaterialParameter {
                    .name = normalize_uniform_name(parameter_binding.name),
                    .data = parameter_binding.value,
                });
        }

        return out_resources.mesh != nullptr;
    }

}
