#pragma once
#include "tbx/systems/async/settings.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"

namespace tbx
{
    /// @brief
    /// Purpose: Stores asynchronous runtime configuration for the application.
    /// @details
    /// Ownership: Value type owned by callers and by AppSettings.
    /// Thread Safety: Safe for concurrent reads; synchronize concurrent writes externally.
    [[serializable]];
    struct TBX_API AsyncSettings
    {
        size worker_count = {};
    };
}
