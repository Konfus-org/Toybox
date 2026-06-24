#pragma once
#include "tbx/systems/world/settings.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"

namespace tbx
{
    /// @brief
    /// Purpose: Defines global world streaming and chunking settings.
    /// @details
    /// Ownership: Owns all configuration values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    [[serializable]];
    struct TBX_API WorldSettings
    {
        Handle startup_world = {};

        float chunk_size = 32.0F;

        uint32 unload_radius_chunks = 6U;
    };
}
