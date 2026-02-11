#pragma once
#include "opengl_resource.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entities.h"
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx::plugins
{
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
        /// Purpose: Loads or retrieves cached OpenGL resources for an entity.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns shared ownership through the output collection and stores shared
        /// ownership in the cache when creation succeeds.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool try_load(const Entity& entity, std::vector<std::shared_ptr<IOpenGlResource>>& out_resources);

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
        /// Ownership: Returns shared ownership of created resources through the output collection.
        /// Thread Safety: Not thread-safe; call only from the render thread.
        /// </remarks>
        bool try_create_resources(
            const Entity& entity,
            std::vector<std::shared_ptr<IOpenGlResource>>& out_resources);

        struct CachedEntityResources
        {
            std::vector<std::shared_ptr<IOpenGlResource>> resources = {};
            Clock::time_point last_use = {};
        };

      private:
        static constexpr std::chrono::seconds UNUSED_TTL = std::chrono::seconds(3);

        AssetManager* _asset_manager = nullptr;
        std::unordered_map<Uuid, CachedEntityResources> _resources_by_entity = {};
    };
}
