#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/component.h"

namespace tbx
{
    /// @brief
    /// Purpose: Describes how a streamed entity should participate in simulation.
    enum class WorldSimulationMode
    {
        FULL,
        REDUCED,
        FROZEN
    };

    TBX_REGISTER_SERIALIZABLE_ENUM(
        WorldSimulationMode,
        {
            {WorldSimulationMode::FULL, "full"},
            {WorldSimulationMode::REDUCED, "reduced"},
            {WorldSimulationMode::FROZEN, "frozen"},
        })

    /// @brief
    /// Purpose: Marks an entity's current simulation state for streaming-aware systems.
    struct TBX_API WorldSimulationState : Component
    {
        WorldSimulationMode mode = WorldSimulationMode::FULL;
    };

    TBX_REGISTER_SERIALIZABLE_STRUCT(WorldSimulationState, id, mode)
}
