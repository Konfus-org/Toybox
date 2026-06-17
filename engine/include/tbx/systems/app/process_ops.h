#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"

namespace tbx
{
    /// @brief
    /// Purpose: Reports whether the OS process with the given id is currently alive.
    /// @details
    /// Ownership: Does not retain any OS handles between calls.
    /// Thread Safety: Safe to call concurrently. On platforms without a process query
    /// (the fallback branch) this conservatively returns true so callers never act on a
    /// false "exited" signal.
    TBX_API bool is_process_running(uint32 pid);
}
