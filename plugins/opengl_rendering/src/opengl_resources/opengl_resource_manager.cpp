#include "opengl_resource_manager.h"
#include "opengl_buffers.h"
#include "opengl_gbuffer.h"
#include "opengl_post_processing_stack_resource.h"
#include "opengl_runtime_resource_descriptor.h"
#include "opengl_shadow_map.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"
#include <cmath>
#include <numbers>
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

    static std::shared_ptr<IOpenGlResource> make_draw_resource_bundle(
        const OpenGlDrawResources& draw_resources)
    {
        return std::make_shared<OpenGlDrawResourceBundle>(draw_resources);
    }

    static Mesh make_panoramic_sky_mesh()
    {
        constexpr int stack_count = 32;
        constexpr int slice_count = 64;

        auto vertices = std::vector<Vertex> {};
        vertices.reserve(
            static_cast<size_t>(stack_count + 1) * static_cast<size_t>(slice_count + 1));

        auto indices = IndexBuffer {};
        indices.reserve(static_cast<size_t>(stack_count * slice_count * 6));

        const float pi = std::numbers::pi_v<float>;
        const float inv_stack_count = 1.0f / static_cast<float>(stack_count);
        const float inv_slice_count = 1.0f / static_cast<float>(slice_count);

        for (int stack = 0; stack <= stack_count; ++stack)
        {
            const float v = static_cast<float>(stack) * inv_stack_count;
            const float theta = v * pi;
            const float sin_theta = std::sin(theta);
            const float cos_theta = std::cos(theta);

            for (int slice = 0; slice <= slice_count; ++slice)
            {
                const float u = static_cast<float>(slice) * inv_slice_count;
                const float phi = u * 2.0f * pi;
                const float sin_phi = std::sin(phi);
                const float cos_phi = std::cos(phi);

                const float x = sin_theta * cos_phi;
                const float y = cos_theta;
                const float z = sin_theta * sin_phi;

                vertices.push_back(
                    Vertex {
                        .position = Vec3(x, y, z),
                        .uv = Vec2(u, 1.0f - v),
                    });
            }
        }

        const int stride = slice_count + 1;
        for (int stack = 0; stack < stack_count; ++stack)
        {
            for (int slice = 0; slice < slice_count; ++slice)
            {
                const uint32 top_left = static_cast<uint32>(stack * stride + slice);
                const uint32 bottom_left = static_cast<uint32>((stack + 1) * stride + slice);
                const uint32 top_right = top_left + 1U;
                const uint32 bottom_right = bottom_left + 1U;

                indices.push_back(top_left);
                indices.push_back(bottom_left);
                indices.push_back(top_right);

                indices.push_back(top_right);
                indices.push_back(bottom_left);
                indices.push_back(bottom_right);
            }
        }

        const VertexBuffer vertex_buffer = {
            vertices,
            {{
                Vec3(0.0f),
                RgbaColor(),
                Vec3(0.0f),
                Vec2(0.0f),
            }}};

        return Mesh(vertex_buffer, indices);
    }

    static Mesh make_fullscreen_quad_mesh()
    {
        const std::vector<Vertex> vertices = {
            Vertex {
                .position = Vec3(-1.0f, -1.0f, 0.0f),
                .uv = Vec2(0.0f, 0.0f),
            },
            Vertex {
                .position = Vec3(1.0f, -1.0f, 0.0f),
                .uv = Vec2(1.0f, 0.0f),
            },
            Vertex {
                .position = Vec3(1.0f, 1.0f, 0.0f),
                .uv = Vec2(1.0f, 1.0f),
            },
            Vertex {
                .position = Vec3(-1.0f, 1.0f, 0.0f),
                .uv = Vec2(0.0f, 1.0f),
            }};

        const IndexBuffer indices = {0, 1, 2, 2, 3, 0};
        const VertexBuffer vertex_buffer = {
            vertices,
            {{
                Vec3(0.0f),
                RgbaColor(),
                Vec3(0.0f),
                Vec2(0.0f),
            }}};
        return Mesh(vertex_buffer, indices);
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

    static void hash_uniform_value(uint64& hash_value, const MaterialParameterData& data)
    {
        constexpr uint64 fnv_prime = 1099511628211ULL;
        auto hash_bytes = [&hash_value](const void* bytes, size_t size)
        {
            const auto* data_bytes = static_cast<const unsigned char*>(bytes);
            for (size_t index = 0; index < size; ++index)
            {
                hash_value ^= static_cast<uint64>(data_bytes[index]);
                hash_value *= fnv_prime;
            }
        };

        const auto variant_index = static_cast<uint64>(data.index());
        hash_bytes(&variant_index, sizeof(variant_index));
        std::visit(
            [&hash_bytes](const auto& value)
            {
                hash_bytes(&value, sizeof(value));
            },
            data);
    }

    static uint64 hash_runtime_material(const MaterialInstance& runtime_material)
    {
        constexpr uint64 fnv_offset = 1469598103934665603ULL;
        constexpr uint64 fnv_prime = 1099511628211ULL;

        auto hash_value = fnv_offset;
        auto hash_bytes = [&hash_value](const void* data, size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                hash_value ^= static_cast<uint64>(bytes[i]);
                hash_value *= fnv_prime;
            }
        };

        hash_bytes(&runtime_material.handle.id, sizeof(runtime_material.handle.id));

        for (const auto& parameter : runtime_material.parameters)
        {
            hash_bytes(parameter.name.data(), parameter.name.size());
            hash_uniform_value(hash_value, parameter.value);
        }

        for (const auto& texture_binding : runtime_material.textures)
        {
            hash_bytes(texture_binding.name.data(), texture_binding.name.size());
            hash_bytes(
                &texture_binding.texture.handle.id,
                sizeof(texture_binding.texture.handle.id));
            const bool has_settings_override = texture_binding.texture.settings.has_value();
            hash_bytes(&has_settings_override, sizeof(has_settings_override));
            if (has_settings_override)
            {
                const TextureSettings& texture_settings = texture_binding.texture.settings.value();
                hash_bytes(&texture_settings.filter, sizeof(texture_settings.filter));
                hash_bytes(&texture_settings.wrap, sizeof(texture_settings.wrap));
                hash_bytes(&texture_settings.format, sizeof(texture_settings.format));
                hash_bytes(&texture_settings.mipmaps, sizeof(texture_settings.mipmaps));
                hash_bytes(&texture_settings.compression, sizeof(texture_settings.compression));
            }
        }

        return hash_value;
    }

    static uint64 hash_runtime_material_resource_identity(const MaterialInstance& runtime_material)
    {
        constexpr uint64 fnv_offset = 1469598103934665603ULL;
        constexpr uint64 fnv_prime = 1099511628211ULL;

        auto hash_value = fnv_offset;
        auto hash_bytes = [&hash_value](const void* data, size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                hash_value ^= static_cast<uint64>(bytes[i]);
                hash_value *= fnv_prime;
            }
        };

        // Cache identity tracks resource-affecting inputs only.
        hash_bytes(&runtime_material.handle.id, sizeof(runtime_material.handle.id));

        for (const auto& texture_binding : runtime_material.textures)
        {
            hash_bytes(texture_binding.name.data(), texture_binding.name.size());
            hash_bytes(
                &texture_binding.texture.handle.id,
                sizeof(texture_binding.texture.handle.id));

            const bool has_settings_override = texture_binding.texture.settings.has_value();
            hash_bytes(&has_settings_override, sizeof(has_settings_override));
            if (has_settings_override)
            {
                const TextureSettings& texture_settings = texture_binding.texture.settings.value();
                hash_bytes(&texture_settings.filter, sizeof(texture_settings.filter));
                hash_bytes(&texture_settings.wrap, sizeof(texture_settings.wrap));
                hash_bytes(&texture_settings.format, sizeof(texture_settings.format));
                hash_bytes(&texture_settings.mipmaps, sizeof(texture_settings.mipmaps));
                hash_bytes(&texture_settings.compression, sizeof(texture_settings.compression));
            }
        }

        return hash_value;
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

    static uint64 hash_runtime_resource_descriptor(
        const OpenGlRuntimeResourceDescriptor& descriptor)
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

        const auto kind_value = static_cast<uint64>(descriptor.kind);
        hash_bytes(&kind_value, sizeof(kind_value));
        hash_bytes(
            &descriptor.shadow_map_resolution.width,
            sizeof(descriptor.shadow_map_resolution.width));
        hash_bytes(
            &descriptor.shadow_map_resolution.height,
            sizeof(descriptor.shadow_map_resolution.height));
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

    static void append_texture_binding(
        OpenGlDrawResources& out_resources,
        std::string_view uniform_name,
        const Texture& texture_data)
    {
        auto texture = std::make_shared<OpenGlTexture>(texture_data);
        auto texture_slot = static_cast<int>(out_resources.textures.size());
        texture->set_slot(static_cast<uint32>(texture_slot));
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
        : _asset_manager(&asset_manager)
    {
        TBX_ASSERT(
            _asset_manager != nullptr,
            "OpenGL resource manager requires a valid asset manager reference.");
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
            auto iterator = _resources_by_entity.find(entity_id);
            if (iterator != _resources_by_entity.end()
                && iterator->second.signature == material_hash
                && try_get_cached_draw_resources(iterator->second, out_resources))
            {
                iterator->second.last_use = Clock::now();
                iterator->second.is_pinned = iterator->second.is_pinned || pin;
                return true;
            }

            auto resources = OpenGlDrawResources {};
            if (!try_create_sky_resources(sky.material, resources))
                return false;

            _resources_by_entity[entity_id] = OpenGlCachedResourceEntry {
                .resource = make_draw_resource_bundle(resources),
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
            auto iterator = _resources_by_entity.find(resource_id);
            if (iterator != _resources_by_entity.end()
                && iterator->second.signature == material_signature
                && iterator->second.aux_signature == mesh_signature
                && try_get_cached_draw_resources(iterator->second, out_resources))
            {
                iterator->second.last_use = Clock::now();
                iterator->second.is_pinned = iterator->second.is_pinned || pin;
                return true;
            }

            if (iterator != _resources_by_entity.end()
                && iterator->second.signature == material_signature && dynamic_mesh.data)
            {
                auto resources = OpenGlDrawResources {};
                if (!try_get_cached_draw_resources(iterator->second, resources))
                    return false;

                resources.mesh = std::make_shared<OpenGlMesh>(*dynamic_mesh.data);
                iterator->second.resource = make_draw_resource_bundle(resources);
                iterator->second.aux_signature = mesh_signature;
                iterator->second.last_use = Clock::now();
                iterator->second.is_pinned = iterator->second.is_pinned || pin;
                out_resources = std::move(resources);
                return true;
            }

            auto resources = OpenGlDrawResources {};
            if (!try_create_dynamic_mesh_resources(entity, renderer, resources))
                return false;

            _resources_by_entity[resource_id] = OpenGlCachedResourceEntry {
                .resource = make_draw_resource_bundle(resources),
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
        auto iterator = _resources_by_entity.find(resource_id);
        if (iterator != _resources_by_entity.end()
            && iterator->second.signature == material_signature
            && iterator->second.aux_signature == model_signature
            && try_get_cached_draw_resources(iterator->second, out_resources))
        {
            iterator->second.last_use = Clock::now();
            iterator->second.is_pinned = iterator->second.is_pinned || pin;
            return true;
        }

        auto resources = OpenGlDrawResources {};
        if (!try_create_static_mesh_resources(entity, renderer, resources))
            return false;

        _resources_by_entity[resource_id] = OpenGlCachedResourceEntry {
            .resource = make_draw_resource_bundle(resources),
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
        if (entity.has_component<OpenGlRuntimeResourceDescriptor>())
        {
            const auto& descriptor = entity.get_component<OpenGlRuntimeResourceDescriptor>();
            return add(entity.get_id(), descriptor, pin);
        }

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

    bool OpenGlResourceManager::add(
        const Uuid& resource_uuid,
        const OpenGlRuntimeResourceDescriptor& descriptor,
        const bool pin)
    {
        if (!resource_uuid.is_valid())
            return false;

        const auto descriptor_hash = hash_runtime_resource_descriptor(descriptor);
        auto iterator = _resources_by_entity.find(resource_uuid);
        if (iterator != _resources_by_entity.end() && iterator->second.signature == descriptor_hash
            && iterator->second.resource)
        {
            iterator->second.last_use = Clock::now();
            iterator->second.is_pinned = iterator->second.is_pinned || pin;
            return true;
        }

        auto created_resource = std::shared_ptr<IOpenGlResource> {};
        switch (descriptor.kind)
        {
            case OpenGlRuntimeResourceKind::GBUFFER:
                created_resource = std::make_shared<OpenGlGBuffer>();
                break;
            case OpenGlRuntimeResourceKind::FRAMEBUFFER:
                created_resource = std::make_shared<OpenGlFrameBuffer>();
                break;
            case OpenGlRuntimeResourceKind::SHADOW_MAP:
                created_resource =
                    std::make_shared<OpenGlShadowMap>(descriptor.shadow_map_resolution);
                break;
        }

        if (!created_resource)
            return false;

        _resources_by_entity[resource_uuid] = OpenGlCachedResourceEntry {
            .resource = created_resource,
            .last_use = Clock::now(),
            .signature = descriptor_hash,
            .is_pinned = pin,
        };
        return true;
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

        if (!add(entity, pin))
            return false;

        entity_iterator = _resources_by_entity.find(resource_id);
        if (entity_iterator != _resources_by_entity.end() && entity_iterator->second.resource)
        {
            out_resource = entity_iterator->second.resource;
            return true;
        }

        return false;
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
        const OpenGlCachedResourceEntry& cached_entry,
        OpenGlDrawResources& out_resources) const
    {
        out_resources = {};
        if (!cached_entry.resource)
            return false;

        auto draw_resources =
            std::dynamic_pointer_cast<OpenGlDrawResourceBundle>(cached_entry.resource);
        if (!draw_resources)
            return false;

        out_resources = draw_resources->get_draw_resources();
        return true;
    }

    void OpenGlResourceManager::pin(const Uuid& resource_uuid)
    {
        if (!resource_uuid.is_valid())
            return;

        auto entity_iterator = _resources_by_entity.find(resource_uuid);
        if (entity_iterator != _resources_by_entity.end())
            entity_iterator->second.is_pinned = true;
    }

    void OpenGlResourceManager::unpin(const Uuid& resource_uuid)
    {
        if (!resource_uuid.is_valid())
            return;

        auto entity_iterator = _resources_by_entity.find(resource_uuid);
        if (entity_iterator != _resources_by_entity.end())
            entity_iterator->second.is_pinned = false;
    }

    void OpenGlResourceManager::clear()
    {
        _resources_by_entity.clear();
        _next_unused_scan_time = {};
    }

    void OpenGlResourceManager::clear_unused()
    {
        const auto now = Clock::now();
        if (_next_unused_scan_time > now)
            return;

        _next_unused_scan_time = now + UNUSED_SCAN_INTERVAL;
        const auto unload_before = Clock::now() - UNUSED_TTL;

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
    }

    bool OpenGlResourceManager::try_create_static_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources)
    {
        const auto& static_mesh = entity.get_component<StaticMesh>();
        auto model = _asset_manager->load<Model>(static_mesh.handle);
        if (!model || model->meshes.empty())
            return false;

        out_resources.mesh = std::make_shared<OpenGlMesh>(model->meshes[0]);

        auto material = Material();
        if (renderer.material.handle.is_valid())
        {
            auto override_material = _asset_manager->load<Material>(renderer.material.handle);
            if (override_material)
                material = *override_material;
        }
        else
        {
            material = resolve_static_mesh_material(*model);
        }

        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(renderer.material, material, &runtime_texture_overrides);
        return try_append_material_resources(
            material,
            runtime_texture_overrides,
            out_resources,
            true);
    }

    bool OpenGlResourceManager::try_create_dynamic_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources)
    {
        const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
        if (!dynamic_mesh.data)
            return false;

        out_resources.mesh = std::make_shared<OpenGlMesh>(*dynamic_mesh.data);

        auto material = Material();
        if (renderer.material.handle.is_valid())
        {
            auto loaded_material = _asset_manager->load<Material>(renderer.material.handle);
            if (loaded_material)
                material = *loaded_material;
        }

        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(renderer.material, material, &runtime_texture_overrides);
        return try_append_material_resources(
            material,
            runtime_texture_overrides,
            out_resources,
            true);
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
            if (!try_create_post_process_resources(effect.material, draw_resources))
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
        OpenGlDrawResources& out_resources)
    {
        if (!material.handle.is_valid())
            return false;
        auto source_material = _asset_manager->load<Material>(material.handle);
        if (!source_material)
            return false;

        static const Mesh skybox_mesh = make_panoramic_sky_mesh();
        out_resources.mesh = std::make_shared<OpenGlMesh>(skybox_mesh);

        auto resolved = *source_material;
        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(material, resolved, &runtime_texture_overrides);
        return try_append_material_resources(
            resolved,
            runtime_texture_overrides,
            out_resources,
            false);
    }

    bool OpenGlResourceManager::try_create_post_process_resources(
        const MaterialInstance& material,
        OpenGlDrawResources& out_resources)
    {
        if (!material.handle.is_valid())
            return false;
        auto source_material = _asset_manager->load<Material>(material.handle);
        if (!source_material)
            return false;

        static const Mesh fullscreen_quad_mesh = make_fullscreen_quad_mesh();
        out_resources.mesh = std::make_shared<OpenGlMesh>(fullscreen_quad_mesh);

        auto resolved = *source_material;
        auto runtime_texture_overrides = std::vector<MaterialTextureBinding> {};
        apply_runtime_material_overrides(material, resolved, &runtime_texture_overrides);
        return try_append_material_resources(
            resolved,
            runtime_texture_overrides,
            out_resources,
            false);
    }

    bool OpenGlResourceManager::try_append_material_resources(
        const Material& material,
        const std::vector<MaterialTextureBinding>& runtime_texture_overrides,
        OpenGlDrawResources& out_resources,
        const bool force_deferred_geometry_program)
    {
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

        if (resolved_material.program.is_valid())
        {
            out_resources.use_tesselation = resolved_material.program.tesselation.is_valid();
            auto shader_stages = std::vector<std::shared_ptr<OpenGlShader>> {};

            const auto append_shader_stages =
                [this, &shader_stages](const Handle& shader_handle, ShaderType expected_type)
            {
                if (!shader_handle.is_valid())
                    return;

                auto shader_asset = _asset_manager->load<Shader>(shader_handle);
                TBX_ASSERT(
                    shader_asset != nullptr,
                    "OpenGL rendering: material shader stage could not be loaded.");
                if (!shader_asset)
                    return;

                const ShaderSource* stage_source = nullptr;
                for (const auto& source : shader_asset->sources)
                {
                    if (source.type != expected_type)
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: shader source type does not match expected stage "
                            "type.");
                        continue;
                    }

                    stage_source = &source;
                    break;
                }

                TBX_ASSERT(
                    stage_source != nullptr,
                    "OpenGL rendering: material shader stage is missing expected source type.");
                if (!stage_source)
                    return;

                auto stage = std::make_shared<OpenGlShader>(*stage_source);
                TBX_ASSERT(
                    stage->get_shader_id() != 0,
                    "OpenGL rendering: material shader stage failed to compile.");
                if (stage->get_shader_id() == 0)
                    return;

                shader_stages.push_back(std::move(stage));
            };

            append_shader_stages(resolved_material.program.vertex, ShaderType::VERTEX);
            append_shader_stages(resolved_material.program.tesselation, ShaderType::TESSELATION);
            append_shader_stages(resolved_material.program.geometry, ShaderType::GEOMETRY);
            append_shader_stages(resolved_material.program.fragment, ShaderType::FRAGMENT);
            append_shader_stages(resolved_material.program.compute, ShaderType::COMPUTE);

            if (!shader_stages.empty())
            {
                out_resources.shader_program = std::make_shared<OpenGlShaderProgram>(shader_stages);
                TBX_ASSERT(
                    out_resources.shader_program->get_program_id() != 0,
                    "OpenGL rendering: failed to link a valid shader program.");
            }
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

                texture_asset = _asset_manager->load(resolved_texture_handle, load_parameters);
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
            append_texture_binding(out_resources, normalized_name, texture_data);
        }

        if (!has_diffuse)
            append_texture_binding(out_resources, "u_diffuse", Texture());
        if (!has_normal)
        {
            auto normal_texture = Texture();
            normal_texture.format = TextureFormat::RGB;
            normal_texture.pixels = {128, 128, 255};
            append_texture_binding(out_resources, "u_normal", normal_texture);
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

        if (!out_resources.shader_program)
            return out_resources.mesh != nullptr;

        auto shader_program_scope = GlResourceScope(*out_resources.shader_program);
        for (const auto& texture_binding : out_resources.textures)
        {
            out_resources.shader_program->try_upload(
                MaterialParameter {
                    .name = texture_binding.uniform_name,
                    .data = texture_binding.slot,
                });
        }

        return out_resources.mesh != nullptr;
    }

}
