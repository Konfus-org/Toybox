#pragma once
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include <string>
#include <unordered_map>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The frosted-glass backdrops' state — per view, the id of the render pass carrying
    /// that view's rect-limited blur post effect (the editor pushes its overlay-card footprints via
    /// view.setGlass) and a hash of the rects it was built from, so an unchanged push is free. Plain
    /// state: glass_ops owns the behavior (parse, rebuild, cleanup).
    /// @details
    /// Ownership: Owned by the plugin by value. Thread Safety: Main thread only.
    struct GlassState
    {
        struct Entry
        {
            tbx::Uuid pass = {};
            uint64 hash = 0U;
        };

        std::unordered_map<std::string, Entry> views = {};
    };
}
