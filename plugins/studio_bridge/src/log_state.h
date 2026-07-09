#pragma once
#include "tbx/types/typedefs.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The log bridge's state: the engine-log listener registration that streams engine
    /// lines to the editor. Plain state: log_ops owns the behavior.
    /// @details
    /// Ownership: Owned by the plugin by value; the listener itself lives on the engine log until
    /// detach_log removes it. Thread Safety: Main-thread only (attached/detached from the plugin's
    /// lifecycle; the listener runs on whatever thread logs).
    struct LogState
    {
        uint listener_id = 0U;
    };
}
