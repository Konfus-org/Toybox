#pragma once
#include "tbx/types/uuid.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor highlight pass's state — the id of the render pass registered on the
    /// engine Rendering service that carries the selection-outline post effect. Plain state:
    /// highlight_ops owns the behavior (pass lifecycle).
    /// @details
    /// Ownership: Owned by the plugin by value. Thread Safety: Main thread only.
    struct HighlightState
    {
        // Id of the highlight pass on the Rendering service, kept so it can be removed before the
        // plugin unloads. Invalid until registered.
        tbx::Uuid pass = {};
    };
}
