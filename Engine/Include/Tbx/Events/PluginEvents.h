#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Plugins/PluginMeta.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class Plugin;

    struct TBX_EXPORT PluginLoadedEvent final : public Event
    {
        PluginLoadedEvent(const WeakRef<Plugin>& pluginRef, const PluginMeta& meta)
            : plugin(pluginRef), Meta(meta) {}

        WeakRef<Plugin> plugin = {};
        PluginMeta Meta = {};
    };

    struct TBX_EXPORT PluginUnloadedEvent final : public Event
    {
        PluginUnloadedEvent(const WeakRef<Plugin>& pluginRef, const PluginMeta& meta)
            : plugin(pluginRef), Meta(meta) {}

        WeakRef<Plugin> plugin = {};
        PluginMeta Meta = {};
    };

    struct TBX_EXPORT PluginDestroyedEvent final : public Event
    {
        PluginDestroyedEvent(const Plugin* pluginPtr)
            : plugin(pluginPtr) {}

        const Plugin* plugin = {};
    };
}
