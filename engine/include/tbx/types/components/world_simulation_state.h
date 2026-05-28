#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/component.h"

namespace tbx
{
    /// @brief
    /// Purpose: Describes how a streamed entity should participate in simulation.
    [[tbx::serializable]];
    enum class WorldSimulationMode
    {
        FULL [[tbx::name("full")]],
        REDUCED [[tbx::name("reduced")]],
        FROZEN [[tbx::name("frozen")]]
    };

    /// @brief
    /// Purpose: Marks an entity's current simulation state for streaming-aware systems.
    [[tbx::serializable]];
    [[tbx::prop(id, mode)]];
    struct TBX_API WorldSimulationState : Component
    {
        WorldSimulationMode mode = WorldSimulationMode::FULL;
    };
}

#include "tbx/types/components/world_simulation_state.generated.h"
