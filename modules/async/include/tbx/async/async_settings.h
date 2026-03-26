#pragma once
#include "job_system.h"

namespace tbx
{

    /// <summary>
    /// Purpose: Stores asynchronous runtime configuration for the application.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type owned by callers and by AppSettings.
    /// Thread Safety: Safe for concurrent reads; synchronize concurrent writes externally.
    /// </remarks>
    struct TBX_API AsyncSettings
    {
        JobSystemConfiguration job_system = {};
    };
}
