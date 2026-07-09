#pragma once
#include "tbx/types/typedefs.h"
#include <unordered_set>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The viewport pick/occlusion queries' state: renderer model handles that failed to
    /// load (dangling/unregistered, e.g. a runtime-only handle), remembered so per-frame picking and
    /// occlusion don't reload — and re-log — them every call. Plain state: picking_ops owns the
    /// behavior.
    /// @details
    /// Ownership: Owned by the plugin by value. Thread Safety: Main-thread only (request handling).
    struct PickingState
    {
        std::unordered_set<uint64> unloadable_models = {};
    };
}
