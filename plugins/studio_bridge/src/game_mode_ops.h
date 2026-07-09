#pragma once
#include <functional>

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct GameModeState;
    struct SyncEventState;

    /// @brief Enters/exits play: snapshot the world then unpause on enter; pause, restore the
    /// snapshot, and reset the simulation systems (physics, scripts) on exit, so a play session
    /// leaves no trace. No-op when already in the requested state. @p set_paused gates the engine's
    /// neutral pause (wired to the plugin's message posting). Entering play binds the editor's
    /// physics-event forwarders onto the live entities (@p events); leaving play forgets them as the
    /// rebuilt world drops the runtime callbacks.
    void set_playing(
        GameModeState& game_mode,
        SyncEventState& events,
        const EngineServices& services,
        const std::function<void(bool paused)>& set_paused,
        bool playing);
}
