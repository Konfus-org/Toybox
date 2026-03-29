#pragma once
#include "job_system.h"

namespace tbx
{

    /// @brief
    /// Purpose: Stores asynchronous runtime configuration for the application.
    /// @details
    /// Ownership: Value type owned by callers and by AppSettings.
    /// Thread Safety: Safe for concurrent reads; synchronize concurrent writes externally.

    struct TBX_API AsyncSettings
    {
        JobSystemConfiguration job_system = {};
    };
}
