#pragma once
#include "opengl_mesh.h"
#include "opengl_resource.h"
#include "opengl_runtime_resource_descriptor.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/shader.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
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
        /// Purpose: Registers one runtime OpenGL resource directly by UUID and descriptor.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores shared ownership of the created resource in this manager cache.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool add(
            const Uuid& resource_uuid,
            const OpenGlRuntimeResourceDescriptor& descriptor,
            bool pin = false);

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
        bool try_get_stored_resource_base(
            const Uuid& resource_uuid,
            std::shared_ptr<IOpenGlResource>& out_resource) const;
        bool try_get_cached_draw_resources(
            const OpenGlCachedResourceEntry& cached_entry,
            OpenGlDrawResources& out_resources) const;
        bool try_create_static_mesh_resources(
            const Entity& entity,
            const Renderer& renderer,
            OpenGlDrawResources& out_resources);
        bool try_create_dynamic_mesh_resources(
            const Entity& entity,
            const Renderer& renderer,
            OpenGlDrawResources& out_resources);
        bool try_create_post_processing_stack_resource(
            const PostProcessing& post_processing,
            std::shared_ptr<IOpenGlResource>& out_resource,
            uint64& out_signature);
        bool try_create_sky_resources(
            const MaterialInstance& sky_material,
            OpenGlDrawResources& out_resources);
        bool try_create_post_process_resources(
            const MaterialInstance& post_process_material,
            OpenGlDrawResources& out_resources);
        bool try_append_material_resources(
            const Material& material,
            const std::vector<MaterialTextureBinding>& runtime_texture_overrides,
            OpenGlDrawResources& out_resources,
            bool force_deferred_geometry_program);

      private:
        using Clock = std::chrono::steady_clock;

        static constexpr std::chrono::seconds UNUSED_TTL = std::chrono::seconds(3);
        static constexpr std::chrono::seconds UNUSED_SCAN_INTERVAL = std::chrono::seconds(1);

        AssetManager* _asset_manager = nullptr;
        std::unordered_map<Uuid, OpenGlCachedResourceEntry> _resources_by_entity = {};
        Clock::time_point _next_unused_scan_time = {};
    };
}
