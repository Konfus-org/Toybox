#pragma once
#include "tbx/assets/assets.h"
#include <filesystem>

namespace tbx::internal
{
    /// @brief
    /// Purpose: Stands the asset system up in one call, the way initialize_reflection stands
    /// reflection up: registers reflection + every serializer, sets the root (and starts
    /// hot-reload watching), then walks the root so every asset file's path and id populate the
    /// identity map up front. Self-guards on is_assets_ready(); run() calls it during boot.
    void initialize_assets(
        AssetsState& state,
        EventsState& events,
        JobsState& jobs,
        std::filesystem::path root);

    /// @brief
    /// Purpose: Unloads assets that have not been referenced (loaded) for longer than
    /// state.idle_lifetime_seconds, announcing each via AssetUnloaded. tbx::run() calls this
    /// every frame; it self-throttles.
    void update_assets(AssetsState& state, EventsState& events);

    /// @brief
    /// Purpose: Unloads every resident asset right now (unconditional update_assets), announcing
    /// each via AssetUnloaded — for tests that need a clean memory slate. Leaves the identity
    /// map, root, and watcher intact, so the subsystem stays initialized (is_assets_ready holds).
    void purge_assets(AssetsState& state, EventsState& events);
}
