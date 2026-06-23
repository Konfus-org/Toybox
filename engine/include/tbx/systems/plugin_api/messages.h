#pragma once
#include "tbx/systems/messaging/message.h"
#include "tbx/tbx_api.h"
#include <string>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Sent synchronously immediately before a plugin (and any dependents) are detached and
    /// their libraries unloaded — e.g. during a hot-reload.
    /// @details
    /// Handlers MUST release any object whose code lives in the unloading module before returning: the
    /// module is still mapped during dispatch, but is unmapped right after. ScriptSystem uses this to
    /// drop its runtime script instances and evict cached script prototypes whose vtables would
    /// otherwise dangle when the scripts' plugin reloads.
    struct TBX_API PluginUnloadingEvent : public Event
    {
        explicit PluginUnloadingEvent(std::string unloading_plugin_name = {})
            : plugin_name(std::move(unloading_plugin_name))
        {
        }

        std::string plugin_name = {};
    };
}
