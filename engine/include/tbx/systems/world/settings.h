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

        /// World-space size of a streamed chunk's cube. Chunks stream in by camera view (a chunk loads
        /// when it's visible to a camera, the same frustum the renderer culls with), so geometry
        /// streams to the camera's render distance and no farther.
        float chunk_size = 32.0F;

        /// World-unit radius around each camera within which streamed chunks stay loaded regardless of
        /// view, so turning around (and nearby shadow casters) don't pop. 0 = view-only.
        float keep_loaded_radius = 64.0F;
    };
}
