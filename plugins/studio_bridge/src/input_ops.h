#pragma once
#include "tbx/systems/time/delta_time.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct InputState;
    struct ViewState;

    /// @brief Unity-style fly controls for the focused editor view: hold right mouse to look while
    /// WASD/QE move, the wheel dollies, and middle-mouse pans. Frame-rate-independent.
    void update_editor_cameras(
        const EngineServices& services, ViewState& views, const tbx::DeltaTime& dt);

    /// @brief Feeds the engine input system from the focused game view while playing, so a game
    /// running under the (hidden) editor window still reacts to input.
    void update_game_input(const EngineServices& services, ViewState& views, bool is_playing);

    /// @brief Reports the playing game's mouse-lock mode to the editor when it changes, so the game
    /// panel can capture/release the cursor to match.
    void report_mouse_lock(InputState& input, const EngineServices& services, bool is_playing);
}
