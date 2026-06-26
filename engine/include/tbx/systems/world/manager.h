#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/world/settings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/handle.h"
#include "tbx/types/vectors.h"
#include <memory>

namespace tbx
{
    class IMessageCoordinator;
    class ThreadManager;

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
            std::weak_ptr<IMessageCoordinator> message_coordinator = {});
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
        /// Purpose: Writes the active world's current entities back to its chunk + globals asset files so
        /// editor edits persist and round-trip on reload. Each loaded chunk keeps its membership; entities
        /// created since load that belong to no chunk are written into the primary (lowest-coord) chunk.
        /// @details
        /// Returns false when there is no asset-backed active world, the asset manager is gone, or a write
        /// fails. Only the active world (loaded from a handle) can be saved.
        bool save_active_world();

        /// @brief
        /// Purpose: Supplies the thread manager that hosts the async world-streaming lane. When unset,
        /// streaming falls back to synchronous chunk loading on the calling thread.
        void set_thread_manager(std::weak_ptr<ThreadManager> thread_manager);

        /// @brief
        /// Purpose: Enables/disables view-based chunk streaming. When disabled, every chunk stays
        /// loaded (no camera-view culling) — used by the editor, whose viewport cameras live outside
        /// the world and so can't drive the world-camera streamer. Enabled by default.
        void set_streaming_enabled(bool enabled);

        /// @brief
        /// Purpose: Updates world streaming for the active world. Call from the application main thread:
        /// it reads cameras and mutates the world (chunk entities); only the chunk-asset deserialize is
        /// offloaded to the streaming lane.
        void update(const DeltaTime& dt, const WorldSettings& settings);

      private:
        void drain_streamed_chunks(World& world);
        void request_chunk_load(World& world, const IVec3& coord);
        void load_all_chunks(World& world);
        bool ensure_streaming_lane(ThreadManager& thread_manager);
        bool refresh_active_world_from_asset();
        bool refresh_world_chunk(const Handle& chunk);
        bool refresh_world_globals();
        std::vector<Entity> collect_runtime_entities(
            const World& world,
            const std::vector<Uuid>& asset_entity_ids);
        bool load_world_globals(World& world);
        void on_asset_reloaded(const AssetReloadedEvent& event);
        void release_active_world();

      private:
        struct State;
        std::unique_ptr<State> _state = {};
    };
}
