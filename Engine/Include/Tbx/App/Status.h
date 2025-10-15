#pragma once
#include "Tbx/DllExport.h"

namespace Tbx
{
    /// <summary>
    /// High level lifecycle states the application can transition through while running.
    /// </summary>
    enum class TBX_EXPORT AppStatus
    {
        None,
        Initializing,
        Running,
        Reloading,
        Paused,
        Minimized,
        Closing,
        Closed,
        Error
    };
}