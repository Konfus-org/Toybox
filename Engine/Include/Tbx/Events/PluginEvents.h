#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Plugins/PluginMeta.h"
#include <utility>
#include <vector>

namespace Tbx
{
    class Plugin;

    struct TBX_EXPORT PluginLoadedEvent final : public Event
    {
        PluginLoadedEvent(WeakRef<Plugin> plugin)
            : Plugin(plugin) {}

        const WeakRef<Plugin> Plugin = {};
    };

    struct TBX_EXPORT PluginUnloadedEvent final : public Event
    {
        PluginUnloadedEvent(const Plugin* plugin)
            : Plugin(plugin) {}

        const Plugin* Plugin = {};
    };
}
