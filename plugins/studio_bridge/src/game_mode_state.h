#pragma once
#include "tbx/systems/ecs/registry.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Play-mode state, which lives in the bridge rather than the engine: the is-playing
    /// flag and the whole-world snapshots captured on entering play and replayed on exit. Plain
    /// state: game_mode_ops owns the behavior (snapshot / restore / simulation reset).
    /// @details
    /// Ownership: Owned by the plugin by value; the snapshots deep-copy the game world, split by
    /// persistence so each entity is restored as runtime or global exactly as it was. Thread Safety:
    /// Main-thread only.
    struct GameModeState
    {
        bool is_playing = false;
        tbx::EntityRegistry runtime_snapshot = {};
        tbx::EntityRegistry global_snapshot = {};
    };
}
