#pragma once
#include "tbx/ecs/sandbox.h"
#include "tbx/utils/typedefs.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Copies a kit's toys under `parent`, expanding nested immediate kits and
    /// registering nested streamed kits — the shared body of spawn() and stream-in. Defined in
    /// kit.cpp with the kit machinery; declared here so stream() (sandbox.cpp) can reach it.
    /// Internal to the ecs implementation — not part of the public sandbox API.
    Result<void> instantiate_under(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        Toy parent,
        const Kit& kit,
        std::vector<uint64>& reference_stack,
        std::vector<ToyId>& spawned);
}
