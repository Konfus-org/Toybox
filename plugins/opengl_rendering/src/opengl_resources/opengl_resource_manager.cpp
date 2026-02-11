#include "opengl_resource_manager.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"
#include <cmath>
#include <numbers>

namespace tbx::plugins
{
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

                vertices.push_back(Vertex {
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

    static std::string normalize_uniform_name(std::string_view name)
    {
        if (name.size() >= 2U && name[0] == 'u' && name[1] == '_')
            return std::string(name);

        std::string normalized = "u_";
        normalized.append(name.begin(), name.end());
        return normalized;
    }

    static void append_or_override_uniform(
        std::vector<ShaderUniform>& uniforms,
        const ShaderUniform& uniform)
    {
        for (auto& existing : uniforms)
        {
            if (existing.name != uniform.name)
                continue;

            existing = uniform;
            return;
        }

        uniforms.push_back(uniform);
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
                .slot = texture_slot,
                .texture = texture,
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

    bool OpenGlResourceManager::try_load(const Entity& entity, OpenGlDrawResources& out_resources)
    {
        const auto resource_id = entity.get_id();
        auto iterator = _resources_by_entity.find(resource_id);
        if (iterator != _resources_by_entity.end())
        {
            iterator->second.last_use = Clock::now();
            out_resources = iterator->second.resources;
            return true;
        }

        auto resources = OpenGlDrawResources {};
        if (!try_create_resources(entity, resources))
            return false;

        _resources_by_entity[resource_id] = CachedEntityResources {
            .resources = resources,
            .last_use = Clock::now(),
        };
        out_resources = std::move(resources);
        return true;
    }

    bool OpenGlResourceManager::try_load_sky(
        const Handle& sky_material,
        OpenGlDrawResources& out_resources)
    {
        if (!sky_material.is_valid())
            return false;

        const auto resource_id = sky_material.id;
        auto iterator = _resources_by_sky_material.find(resource_id);
        if (iterator != _resources_by_sky_material.end())
        {
            iterator->second.last_use = Clock::now();
            out_resources = iterator->second.resources;
            return true;
        }

        auto resources = OpenGlDrawResources {};
        if (!try_create_sky_resources(sky_material, resources))
            return false;

        _resources_by_sky_material[resource_id] = CachedSkyResources {
            .resources = resources,
            .last_use = Clock::now(),
        };
        out_resources = std::move(resources);
        return true;
    }

    void OpenGlResourceManager::clear()
    {
        _resources_by_entity.clear();
        _resources_by_sky_material.clear();
    }

    void OpenGlResourceManager::unload_unreferenced()
    {
        const auto unload_before = Clock::now() - UNUSED_TTL;

        for (auto iterator = _resources_by_entity.begin(); iterator != _resources_by_entity.end();)
        {
            if (iterator->second.last_use >= unload_before)
            {
                ++iterator;
                continue;
            }

            iterator = _resources_by_entity.erase(iterator);
        }

        for (auto iterator = _resources_by_sky_material.begin();
             iterator != _resources_by_sky_material.end();)
        {
            if (iterator->second.last_use >= unload_before)
            {
                ++iterator;
                continue;
            }

            iterator = _resources_by_sky_material.erase(iterator);
        }
    }

    bool OpenGlResourceManager::try_create_resources(
        const Entity& entity,
        OpenGlDrawResources& out_resources)
    {
        const auto& renderer = entity.get_component<Renderer>();
        if (entity.has_component<StaticMesh>())
            return try_create_static_mesh_resources(entity, renderer, out_resources);
        if (entity.has_component<DynamicMesh>())
            return try_create_dynamic_mesh_resources(entity, renderer, out_resources);

        return false;
    }

    bool OpenGlResourceManager::try_create_static_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources)
    {
        const auto& static_mesh = entity.get_component<StaticMesh>();
        auto model = _asset_manager->load<Model>(static_mesh.model);
        if (!model || model->meshes.empty())
            return false;

        out_resources.mesh = std::make_shared<OpenGlMesh>(model->meshes[0]);

        auto material = Material();
        if (renderer.material.is_valid())
        {
            auto override_material = _asset_manager->load<Material>(renderer.material);
            if (override_material)
                material = *override_material;
        }
        else
        {
            material = resolve_static_mesh_material(*model);
        }

        return try_append_material_resources(material, out_resources);
    }

    bool OpenGlResourceManager::try_create_dynamic_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources)
    {
        const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
        if (!dynamic_mesh.mesh)
            return false;

        out_resources.mesh = std::make_shared<OpenGlMesh>(*dynamic_mesh.mesh);

        auto material = Material();
        if (renderer.material.is_valid())
        {
            auto loaded_material = _asset_manager->load<Material>(renderer.material);
            if (loaded_material)
                material = *loaded_material;
        }

        return try_append_material_resources(material, out_resources);
    }

    bool OpenGlResourceManager::try_create_sky_resources(
        const Handle& sky_material,
        OpenGlDrawResources& out_resources)
    {
        auto material = _asset_manager->load<Material>(sky_material);
        if (!material)
            return false;

        static const Mesh skybox_mesh = make_panoramic_sky_mesh();
        out_resources.mesh = std::make_shared<OpenGlMesh>(skybox_mesh);
        return try_append_material_resources(*material, out_resources);
    }

    bool OpenGlResourceManager::try_append_material_resources(
        const Material& material,
        OpenGlDrawResources& out_resources)
    {
        auto resolved_material = material;
        if (!resolved_material.shader.is_valid())
        {
            resolved_material.shader.vertex = lit_vertex_shader;
            resolved_material.shader.fragment = lit_fragment_shader;
        }

        if (resolved_material.shader.is_valid())
        {
            out_resources.use_tesselation = resolved_material.shader.tesselation.is_valid();
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

            append_shader_stages(resolved_material.shader.vertex, ShaderType::VERTEX);
            append_shader_stages(resolved_material.shader.tesselation, ShaderType::TESSELATION);
            append_shader_stages(resolved_material.shader.geometry, ShaderType::GEOMETRY);
            append_shader_stages(resolved_material.shader.fragment, ShaderType::FRAGMENT);
            append_shader_stages(resolved_material.shader.compute, ShaderType::COMPUTE);

            if (!shader_stages.empty())
            {
                out_resources.shader_program =
                    std::make_shared<OpenGlShaderProgram>(shader_stages);
                TBX_ASSERT(
                    out_resources.shader_program->get_program_id() != 0,
                    "OpenGL rendering: failed to link a valid shader program.");
            }
        }

        bool has_diffuse = false;
        bool has_normal = false;
        for (const auto& [texture_name, texture_handle] : resolved_material.textures)
        {
            const auto normalized_name = normalize_uniform_name(texture_name);
            has_diffuse = has_diffuse || normalized_name == "u_diffuse";
            has_normal = has_normal || normalized_name == "u_normal";

            std::shared_ptr<Texture> texture_asset = nullptr;
            if (texture_handle.is_valid())
                texture_asset = _asset_manager->load<Texture>(texture_handle);

            auto fallback_texture = Texture();
            if (normalized_name == "u_normal")
            {
                fallback_texture.format = TextureFormat::RGB;
                fallback_texture.pixels = {128, 128, 255};
            }

            const Texture& texture_data = texture_asset ? *texture_asset : fallback_texture;
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

        for (const auto& [parameter_name, parameter_value] : resolved_material.parameters)
        {
            append_or_override_uniform(
                out_resources.shader_parameters,
                ShaderUniform {
                    .name = normalize_uniform_name(parameter_name),
                    .data = parameter_value,
                });
        }

        if (!out_resources.shader_program)
            return out_resources.mesh != nullptr;

        auto shader_program_scope = GlResourceScope(*out_resources.shader_program);
        for (const auto& texture_binding : out_resources.textures)
        {
            out_resources.shader_program->try_upload(
                ShaderUniform {
                    .name = texture_binding.uniform_name,
                    .data = texture_binding.slot,
                });
        }

        return out_resources.mesh != nullptr;
    }
}
