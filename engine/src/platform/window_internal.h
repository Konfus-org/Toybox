#pragma once
#include "tbx/platform/window.h"

namespace tbx::internal
{
    /// @brief
    /// Purpose: Runs the windows for one frame: presents what was drawn since the last call,
    /// materializes OS windows for new entries (the first brings up the shared GL context),
    /// applies changed data (title, icon loaded from its Texture asset, cursor mode, vsync), and
    /// pumps OS events into the input state and event signals. Called by tbx::run() every frame.
    void update_windows(
        WindowsState& state,
        InputState& input,
        EventsState& events,
        AssetsState& assets);

    /// @brief
    /// Purpose: Binds the shared GL context to this window's surface — subsequent gpu calls
    /// draw into it. Called per window by the render loop; no-op before the first update_windows().
    void make_current(const Window& window);
}
