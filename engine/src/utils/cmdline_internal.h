#pragma once

namespace tbx::internal
{
    struct RuntimeState;

    /// @brief
    /// Purpose: Per-frame command handling (--screenshot captures) — run() calls it at
    /// frame start, while the backbuffer still holds the previous frame's image.
    void update_cmdline(RuntimeState& state);
}
