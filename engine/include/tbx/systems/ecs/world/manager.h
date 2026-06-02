#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/reload_queue.h"
#include "tbx/systems/ecs/world/settings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/handle.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Owns the active world reference and coordinates world streaming.
    /// @details
    /// Ownership: Holds the active world strongly and exposes weak references to callers.
    /// Thread Safety: Not thread-safe; call from the application main thread.
    class TBX_API WorldManager final
    {
      public:
        WorldManager(
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<AssetReloadQueue> reload_queue = {});
        ~WorldManager() noexcept;

      public:
        WorldManager(const WorldManager&) = delete;
        WorldManager& operator=(const WorldManager&) = delete;
        WorldManager(WorldManager&&) noexcept = delete;
        WorldManager& operator=(WorldManager&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Clears the current active world and releases manager-owned asset references.
        void clear_active_world();

        /// @brief
        /// Purpose: Returns the active world handle when the world came from an asset handle.
        Handle get_active_world_handle() const;

        /// @brief
        /// Purpose: Returns a weak reference to the active world.
        std::weak_ptr<World> get_active_world() const;

        /// @brief
        /// Purpose: Returns whether a usable active world is assigned.
        bool has_active_world() const;

        /// @brief
        /// Purpose: Loads and activates a world asset, preserving the current world on failure.
        bool set_active_world(const Handle& handle);

        /// @brief
        /// Purpose: Activates an externally supplied world instance.
        bool set_active_world(std::shared_ptr<World> world);

        /// @brief
        /// Purpose: Updates world streaming for the active world.
        void update(const DeltaTime& dt, const WorldSettings& settings);

      private:
        bool refresh_active_world_from_asset();
        bool refresh_world_chunk(const Handle& chunk);
        bool refresh_world_globals();
        std::vector<Entity> collect_runtime_entities(
            const World& world,
            const std::vector<Uuid>& asset_entity_ids);
        bool load_world_globals(World& world);
        void on_asset_reload(const AssetReloadContext& context);
        void release_active_world();

      private:
        struct State;
        std::unique_ptr<State> _state = {};
    };
}
