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
            : LoadedPlugin(pluginRef), LoadedMeta(meta) {}

        WeakRef<Plugin> LoadedPlugin = {};
        PluginMeta LoadedMeta = {};
    };

    struct TBX_EXPORT PluginUnloadedEvent final : public Event
    {
        PluginUnloadedEvent(const WeakRef<Plugin>& pluginRef, const PluginMeta& meta)
            : UnloadedPlugin(pluginRef), UnloadedMeta(meta) {}

        WeakRef<Plugin> UnloadedPlugin = {};
        PluginMeta UnloadedMeta = {};
    };

    struct TBX_EXPORT PluginDestroyedEvent final : public Event
    {
        PluginDestroyedEvent(const Plugin* pluginPtr)
            : DestroyedPlugin(pluginPtr) {}

        const Plugin* DestroyedPlugin = {};
    };
}
