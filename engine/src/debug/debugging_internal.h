#pragma once

namespace tbx::internal
{
    struct RuntimeState;

    /// @brief
    /// Purpose: Runs the debug overlay for one frame from the whole runtime — F3 toggles it,
    /// the first open loads its document, and its stats land in the ui bindings. Called by
    /// tbx::run() every frame.
    void update_debugging(RuntimeState& state);
}
