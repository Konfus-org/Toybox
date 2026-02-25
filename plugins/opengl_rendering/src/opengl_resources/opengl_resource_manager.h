#pragma once
#include "opengl_mesh.h"
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/shader.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Describes a texture-to-sampler binding for an OpenGL shader program.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the uniform name string and shared ownership of the texture resource.
    /// Thread Safety: Not thread-safe; configure and use on the render thread.
    /// </remarks>
    struct OpenGlTextureBinding final
    {
        std::string uniform_name = "";
        std::shared_ptr<OpenGlTexture> texture = nullptr;
        int slot = 0;
    };

    /// <summary>
    /// Purpose: Groups the OpenGL resources needed to draw one renderable entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Shares ownership of mesh, shader program, and textures.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlDrawResources final
    {
        std::shared_ptr<OpenGlMesh> mesh = nullptr;
        std::shared_ptr<OpenGlShaderProgram> shader_program = nullptr;
        std::vector<OpenGlTextureBinding> textures = {};
        std::vector<MaterialParameter> shader_parameters = {};
        bool use_tesselation = false;
    };

    /// <summary>
    /// Purpose: Stores one cached OpenGL resource and eviction metadata.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores shared ownership of one IOpenGlResource instance.
    /// Thread Safety: Not thread-safe; mutate only on the render thread.
    /// </remarks>
    struct OpenGlCachedResourceEntry final
    {
        std::shared_ptr<IOpenGlResource> resource = nullptr;
        std::chrono::steady_clock::time_point last_use = {};
        uint64 signature = 0U;
        uint64 aux_signature = 0U;
        bool is_pinned = false;
    };

    /// <summary>
    /// Purpose: Stores one hash reference to a cached OpenGL texture.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the uniform name and stores a non-owning cache signature.
    /// Thread Safety: Not thread-safe; mutate only on the render thread.
    /// </remarks>
    struct OpenGlTextureResourceReference final
    {
        std::string uniform_name = "";
        uint64 texture_signature = 0U;
        int slot = 0;
    };

    /// <summary>
    /// Purpose: Stores hash references for one cached draw-resource payload.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores value-type signatures and owned uniform/texture metadata.
    /// Thread Safety: Not thread-safe; mutate only on the render thread.
    /// </remarks>
    struct OpenGlCachedDrawResourceEntry final
    {
        uint64 mesh_signature = 0U;
        uint64 shader_program_signature = 0U;
        std::vector<OpenGlTextureResourceReference> textures = {};
        std::vector<MaterialParameter> shader_parameters = {};
        bool use_tesselation = false;
        std::chrono::steady_clock::time_point last_use = {};
        uint64 signature = 0U;
        uint64 aux_signature = 0U;
        bool is_pinned = false;
    };

    /// <summary>
    /// Purpose: Stores one typed OpenGL resource cache entry with usage timestamp.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores shared ownership of one typed OpenGL resource.
    /// Thread Safety: Not thread-safe; mutate only on the render thread.
    /// </remarks>
    template <typename TResource>
    struct OpenGlSharedResourceCacheEntry final
    {
        std::shared_ptr<TResource> resource = nullptr;
        std::chrono::steady_clock::time_point last_use = {};
    };

    /// <summary>
    /// Purpose: Owns OpenGL resource caches and unloads entries that have not been referenced
    /// recently.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores shared ownership of cached resources keyed by entity UUID and stores a
    /// non-owning reference to AssetManager for source asset loading.
    /// Thread Safety: Not thread-safe; call only from the render thread.
    /// </remarks>
    class OpenGlResourceManager final
    {
      public:
        /// <summary>
        /// Purpose: Initializes the resource manager with an asset manager dependency.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning pointer to the asset manager, which must outlive this
        /// manager.
        /// Thread Safety: Not thread-safe; construct on the render thread.
        /// </remarks>
        explicit OpenGlResourceManager(AssetManager& asset_manager);

        /// <summary>
        /// Purpose: Loads or retrieves cached OpenGL draw resources for an entity.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns shared ownership through the output struct and stores shared
        /// ownership in the cache when creation succeeds.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool try_get(const Entity& entity, OpenGlDrawResources& out_resources, bool pin = false);

        /// <summary>
        /// Purpose: Registers cached OpenGL resources for an entity without requiring immediate
        /// retrieval.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores shared ownership of created resources in this manager cache.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool add(const Entity& entity, bool pin = false);

        /// <summary>
        /// Purpose: Registers one runtime OpenGL resource by UUID and constructor arguments.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores shared ownership of the created resource in this manager cache.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        template <typename ResourceType, typename... TArgs>
        bool add(const Uuid& resource_uuid, TArgs&&... args);

        /// <summary>
        /// Purpose: Registers one runtime OpenGL resource with an auto-generated UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores shared ownership of the created resource in this manager cache and
        /// returns the generated UUID.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        template <typename ResourceType, typename... TArgs>
        Uuid add(TArgs&&... args);

        /// <summary>
        /// Purpose: Resolves one cached resource by entity.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns shared ownership through output parameter when found.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool try_get(
            const Entity& entity,
            std::shared_ptr<IOpenGlResource>& out_resource,
            bool pin = false);

        /// <summary>
        /// Purpose: Resolves one stored resource by UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns shared ownership through output parameter when found.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool try_get(const Uuid& resource_uuid, std::shared_ptr<IOpenGlResource>& out_resource)
            const;

        /// <summary>
        /// Purpose: Marks one stored runtime resource as pinned.
        /// </summary>
        /// <remarks>
        /// Ownership: Adds the UUID to the pinned-resource set.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        void pin(const Uuid& resource_uuid);

        /// <summary>
        /// Purpose: Removes one runtime resource pin.
        /// </summary>
        /// <remarks>
        /// Ownership: Removes the UUID from the pinned-resource set.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        void unpin(const Uuid& resource_uuid);

        /// <summary>
        /// Purpose: Clears every cached resource entry.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases this manager's shared ownership of all resources.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        void clear();

        /// <summary>
        /// Purpose: Removes cached resources whose last reference time exceeds the configured TTL.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases shared ownership of resources removed from non-pinned cache maps.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        void clear_unused();

      private:
        template <typename ResourceType, typename... TArgs>
        bool add_resource_with_uuid(const Uuid& resource_uuid, TArgs&&... args);

        static void append_signature_bytes(uint64& hash_value, const void* data, size_t size);

        template <typename TValue>
        static void append_signature_value(uint64& hash_value, const TValue& value);

        template <typename ResourceType, typename... TArgs>
        static uint64 hash_runtime_resource_signature(const TArgs&... args);

        bool try_get_stored_resource_base(
            const Uuid& resource_uuid,
            std::shared_ptr<IOpenGlResource>& out_resource) const;
        bool try_get_cached_draw_resources(
            const OpenGlCachedDrawResourceEntry& cached_entry,
            OpenGlDrawResources& out_resources);
        std::shared_ptr<OpenGlMesh> get_or_create_shared_mesh(
            uint64 mesh_signature,
            const Mesh& mesh);
        std::shared_ptr<OpenGlShader> get_or_create_shared_shader_stage(
            const Handle& shader_handle,
            ShaderType expected_type);
        std::shared_ptr<OpenGlShaderProgram> get_or_create_shared_shader_program(
            const ShaderProgram& shader_program,
            uint64* out_program_signature);
        std::shared_ptr<OpenGlTexture> get_or_create_shared_texture(
            uint64 texture_signature,
            const Texture& texture_data);
        bool try_create_static_mesh_resources(
            const Entity& entity,
            const Renderer& renderer,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);
        bool try_create_dynamic_mesh_resources(
            const Entity& entity,
            const Renderer& renderer,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);
        bool try_create_post_processing_stack_resource(
            const PostProcessing& post_processing,
            std::shared_ptr<IOpenGlResource>& out_resource,
            uint64& out_signature);
        bool try_create_sky_resources(
            const MaterialInstance& sky_material,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);
        bool try_create_post_process_resources(
            const MaterialInstance& post_process_material,
            OpenGlDrawResources& out_resources,
            OpenGlCachedDrawResourceEntry* out_cache_entry);
        bool try_append_material_resources(
            const Material& material,
            const std::vector<MaterialTextureBinding>& runtime_texture_overrides,
            OpenGlDrawResources& out_resources,
            uint64* out_shader_program_signature,
            std::vector<OpenGlTextureResourceReference>* out_texture_references,
            bool force_deferred_geometry_program);

      private:
        using Clock = std::chrono::steady_clock;

        static constexpr std::chrono::seconds UNUSED_TTL = std::chrono::seconds(3);
        static constexpr std::chrono::seconds UNUSED_SCAN_INTERVAL = std::chrono::seconds(1);

        std::reference_wrapper<AssetManager> _asset_manager;
        std::unordered_map<Uuid, OpenGlCachedDrawResourceEntry> _draw_resources_by_entity = {};
        std::unordered_map<Uuid, OpenGlCachedResourceEntry> _resources_by_entity = {};
        std::unordered_map<uint64, OpenGlSharedResourceCacheEntry<OpenGlMesh>>
            _shared_meshes_by_signature = {};
        std::unordered_map<uint64, OpenGlSharedResourceCacheEntry<OpenGlShader>>
            _shared_shader_stages_by_signature = {};
        std::unordered_map<uint64, OpenGlSharedResourceCacheEntry<OpenGlShaderProgram>>
            _shared_shader_programs_by_signature = {};
        std::unordered_map<uint64, OpenGlSharedResourceCacheEntry<OpenGlTexture>>
            _shared_textures_by_signature = {};
        Clock::time_point _next_unused_scan_time = {};
    };

    template <typename ResourceType, typename... TArgs>
    bool OpenGlResourceManager::add(const Uuid& resource_uuid, TArgs&&... args)
    {
        return add_resource_with_uuid<ResourceType>(resource_uuid, std::forward<TArgs>(args)...);
    }

    template <typename ResourceType, typename... TArgs>
    Uuid OpenGlResourceManager::add(TArgs&&... args)
    {
        auto resource_uuid = Uuid::generate();
        if (!add_resource_with_uuid<ResourceType>(resource_uuid, std::forward<TArgs>(args)...))
            return Uuid::NONE;

        return resource_uuid;
    }

    template <typename ResourceType, typename... TArgs>
    bool OpenGlResourceManager::add_resource_with_uuid(const Uuid& resource_uuid, TArgs&&... args)
    {
        static_assert(
            std::is_base_of_v<IOpenGlResource, ResourceType>,
            "ResourceType must derive from IOpenGlResource.");

        if (!resource_uuid.is_valid())
            return false;

        const uint64 signature =
            hash_runtime_resource_signature<ResourceType>(std::forward<TArgs>(args)...);
        auto iterator = _resources_by_entity.find(resource_uuid);
        if (iterator != _resources_by_entity.end() && iterator->second.signature == signature
            && iterator->second.resource)
        {
            iterator->second.last_use = Clock::now();
            return true;
        }

        _resources_by_entity[resource_uuid] = OpenGlCachedResourceEntry {
            .resource = std::make_shared<ResourceType>(std::forward<TArgs>(args)...),
            .last_use = Clock::now(),
            .signature = signature,
        };
        return true;
    }

    template <typename TValue>
    void OpenGlResourceManager::append_signature_value(uint64& hash_value, const TValue& value)
    {
        static_assert(
            std::is_trivially_copyable_v<TValue>,
            "Runtime resource signatures only support trivially copyable arguments.");
        append_signature_bytes(hash_value, &value, sizeof(value));
    }

    template <typename ResourceType, typename... TArgs>
    uint64 OpenGlResourceManager::hash_runtime_resource_signature(const TArgs&... args)
    {
        constexpr uint64 fnv_offset = 1469598103934665603ULL;

        auto hash_value = fnv_offset;
        const auto type_hash = static_cast<uint64>(typeid(ResourceType).hash_code());
        append_signature_bytes(hash_value, &type_hash, sizeof(type_hash));
        (append_signature_value(hash_value, args), ...);
        return hash_value;
    }
}
