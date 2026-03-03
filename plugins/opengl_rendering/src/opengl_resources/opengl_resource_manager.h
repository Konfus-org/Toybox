#pragma once
#include "opengl_mesh.h"
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/post_processing.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/shader.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace opengl_rendering
{
    struct OpenGlTextureBinding final
    {
        std::string uniform_name = {};
        std::shared_ptr<OpenGlTexture> texture = nullptr;
        int slot = 0;
    };

    struct OpenGlDrawResources final
    {
        std::shared_ptr<OpenGlMesh> mesh = nullptr;
        std::shared_ptr<OpenGlShaderProgram> shader_program = nullptr;
        std::vector<OpenGlTextureBinding> textures = {};
        std::vector<tbx::MaterialParameter> shader_parameters = {};
        bool use_tesselation = false;
    };

    class OpenGlResourceManager final
    {
      public:
        explicit OpenGlResourceManager(tbx::AssetManager& asset_manager);

        bool try_get(const tbx::Entity& entity, OpenGlDrawResources& out_resources, bool pin = false);
        bool add(const tbx::Entity& entity, bool pin = false);
        bool try_get(const tbx::Uuid& resource_uuid, std::shared_ptr<IOpenGlResource>& out_resource) const;
        bool try_get(const tbx::Entity& entity, std::shared_ptr<IOpenGlResource>& out_resource, bool pin = false);

        bool try_get_or_create_shader_program(
            const tbx::ShaderProgram& shader_program,
            std::shared_ptr<OpenGlShaderProgram>& out_program);

        std::shared_ptr<OpenGlMesh> get_or_create_runtime_mesh(const tbx::uint64 mesh_signature, const tbx::Mesh& mesh);

        void pin(const tbx::Uuid& resource_uuid);
        void unpin(const tbx::Uuid& resource_uuid);
        void clear();
        void clear_unused();

      private:
        template <typename TResource>
        struct OpenGlSharedResourceCacheEntry final
        {
            std::shared_ptr<TResource> resource = nullptr;
            std::chrono::steady_clock::time_point last_use = {};
        };

        struct OpenGlTextureResourceReference final
        {
            std::string uniform_name = {};
            tbx::uint64 texture_signature = 0U;
            int slot = 0;
        };

        struct OpenGlCachedDrawResourceEntry final
        {
            tbx::uint64 mesh_signature = 0U;
            tbx::uint64 shader_program_signature = 0U;
            std::vector<OpenGlTextureResourceReference> textures = {};
            std::vector<tbx::MaterialParameter> shader_parameters = {};
            bool use_tesselation = false;
            std::chrono::steady_clock::time_point last_use = {};
            tbx::uint64 signature = 0U;
            tbx::uint64 aux_signature = 0U;
            bool is_pinned = false;
        };

        struct OpenGlCachedResourceEntry final
        {
            std::shared_ptr<IOpenGlResource> resource = nullptr;
            std::chrono::steady_clock::time_point last_use = {};
            tbx::uint64 signature = 0U;
            tbx::uint64 aux_signature = 0U;
            bool is_pinned = false;
        };

        static void append_signature_bytes(tbx::uint64& hash_value, const void* data, size_t size);

        bool try_get_stored_resource_base(
            const tbx::Uuid& resource_uuid,
            std::shared_ptr<IOpenGlResource>& out_resource) const;

        bool try_get_cached_draw_resources(
            const OpenGlCachedDrawResourceEntry& cached_entry,
            OpenGlDrawResources& out_resources);

        std::shared_ptr<OpenGlMesh> get_or_create_shared_mesh(
            tbx::uint64 mesh_signature,
            const tbx::Mesh& mesh);

        std::shared_ptr<OpenGlShader> get_or_create_shared_shader_stage(
            const tbx::Handle& shader_handle,
            tbx::ShaderType expected_type);

        std::shared_ptr<OpenGlShaderProgram> get_or_create_shared_shader_program(
            const tbx::ShaderProgram& shader_program,
            tbx::uint64* out_program_signature);

        std::shared_ptr<OpenGlTexture> get_or_create_shared_texture(
            tbx::uint64 texture_signature,
            const tbx::Texture& texture_data);

        bool try_create_static_mesh_resources(
            const tbx::Entity& entity,
            const tbx::Renderer& renderer,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);

        bool try_create_dynamic_mesh_resources(
            const tbx::Entity& entity,
            const tbx::Renderer& renderer,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);

        bool try_create_post_processing_stack_resource(
            const tbx::PostProcessing& post_processing,
            std::shared_ptr<IOpenGlResource>& out_resource,
            tbx::uint64& out_signature);

        bool try_create_sky_resources(
            const tbx::MaterialInstance& material,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);

        bool try_create_post_process_resources(
            const tbx::MaterialInstance& material,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);

        bool try_append_material_resources(
            const tbx::Material& material,
            const std::vector<tbx::MaterialTextureBinding>& runtime_texture_overrides,
            OpenGlDrawResources& out_resources,
            tbx::uint64* out_shader_program_signature,
            std::vector<OpenGlTextureResourceReference>* out_texture_references,
            bool force_deferred_geometry_program);

      private:
        using Clock = std::chrono::steady_clock;

        static constexpr auto UNUSED_TTL = std::chrono::seconds(5);
        static constexpr auto UNUSED_SCAN_INTERVAL = std::chrono::seconds(2);

        std::reference_wrapper<tbx::AssetManager> _asset_manager;
        Clock::time_point _next_unused_scan_time = {};

        std::unordered_map<tbx::Uuid, OpenGlCachedDrawResourceEntry> _draw_resources_by_entity = {};
        std::unordered_map<tbx::Uuid, OpenGlCachedResourceEntry> _resources_by_entity = {};
        std::unordered_map<tbx::uint64, OpenGlSharedResourceCacheEntry<OpenGlMesh>> _shared_meshes_by_signature = {};
        std::unordered_map<tbx::uint64, OpenGlSharedResourceCacheEntry<OpenGlShader>> _shared_shader_stages_by_signature = {};
        std::unordered_map<tbx::uint64, OpenGlSharedResourceCacheEntry<OpenGlShaderProgram>> _shared_shader_programs_by_signature = {};
        std::unordered_map<tbx::uint64, OpenGlSharedResourceCacheEntry<OpenGlTexture>> _shared_textures_by_signature = {};
    };
}
