#pragma once
#include "opengl_mesh.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entities.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/shader.h"
#include <chrono>
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
        int slot = 0;
        std::shared_ptr<OpenGlTexture> texture = nullptr;
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
        bool use_tesselation = false;
        std::vector<OpenGlTextureBinding> textures = {};
        std::vector<ShaderUniform> shader_parameters = {};
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
        bool try_load(const Entity& entity, OpenGlDrawResources& out_resources);

        /// <summary>
        /// Purpose: Loads or retrieves cached OpenGL draw resources for a sky material.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns shared ownership through the output struct and stores shared
        /// ownership in the cache when creation succeeds.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool try_load_sky(const Handle& sky_material, OpenGlDrawResources& out_resources);

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
        /// Ownership: Releases shared ownership of resources removed from cache maps.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        void unload_unreferenced();

      private:
        using Clock = std::chrono::steady_clock;

        /// <summary>
        /// Purpose: Creates OpenGL resources for an entity from the source TBX assets.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns shared ownership of created resources through the output struct.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool try_create_resources(const Entity& entity, OpenGlDrawResources& out_resources);
        bool try_create_static_mesh_resources(
            const Entity& entity,
            const Renderer& renderer,
            OpenGlDrawResources& out_resources);
        bool try_create_dynamic_mesh_resources(
            const Entity& entity,
            const Renderer& renderer,
            OpenGlDrawResources& out_resources);
        bool try_create_sky_resources(
            const Handle& sky_material,
            OpenGlDrawResources& out_resources);
        bool try_append_material_resources(
            const Material& material,
            OpenGlDrawResources& out_resources);

        struct CachedEntityResources final
        {
            OpenGlDrawResources resources = {};
            Clock::time_point last_use = {};
        };

        struct CachedSkyResources final
        {
            OpenGlDrawResources resources = {};
            Clock::time_point last_use = {};
        };

      private:
        static constexpr std::chrono::seconds UNUSED_TTL = std::chrono::seconds(3);

        AssetManager* _asset_manager = nullptr;
        std::unordered_map<Uuid, CachedEntityResources> _resources_by_entity = {};
        std::unordered_map<Uuid, CachedSkyResources> _resources_by_sky_material = {};
    };
}
