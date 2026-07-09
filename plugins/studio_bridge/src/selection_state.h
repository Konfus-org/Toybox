#pragma once
#include "tbx/types/uuid.h"
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor's current selection set — the entity ids the viewports highlight and the
    /// gizmo transforms. Plain state: selection_ops owns the behavior (applying a pushed set and
    /// swapping the runtime selected tag).
    /// @details
    /// Ownership: Owned by the plugin by value. Thread Safety: Main-thread only (written from
    /// request handling, read from on_update).
    struct SelectionState
    {
        std::vector<tbx::Uuid> ids = {};
    };
}
