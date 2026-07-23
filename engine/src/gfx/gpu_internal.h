#pragma once

namespace tbx::internal
{
    /// @brief
    /// Purpose: Initializes the selected backend's GPU access; must run once after window
    /// creation. Each backend loads its functions its own way — no platform types leak here.
    /// The window backend calls it when it brings up the first window during run().
    void initialize_rendering();
}
