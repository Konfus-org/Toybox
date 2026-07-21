#pragma once
#include "tbx/utils/api.h"

namespace tbx
{
    struct App;
}

// The engine's debug overlay: frame timing, world and asset counts, rendered through the UI
// stack. tbx::run() toggles it with F3; tools may drive it directly.
namespace tbx::debug
{
    /// @brief
    /// Purpose: Draws the overlay when open — called by the render graph's ui pass so it
    /// lands inside the frame.
    TBX_API void draw();

    /// @brief
    /// Purpose: Whether the overlay is currently shown.
    TBX_API bool is_open();

    /// @brief
    /// Purpose: Clears the overlay state; run() calls this at shutdown.
    TBX_API void reset();

    /// @brief
    /// Purpose: Shows or hides the overlay (loads it on first show).
    TBX_API void set_open(bool is_open);

    /// @brief
    /// Purpose: Flips the overlay.
    TBX_API void toggle();

    /// @brief
    /// Purpose: Refreshes the overlay's numbers; called by tbx::run() every frame.
    TBX_API void update(const App& app);
}
