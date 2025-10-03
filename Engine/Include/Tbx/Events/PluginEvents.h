#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Plugins/PluginMeta.h"
#include <utility>
#include <vector>

namespace Tbx
{
    class Plugin;

    class TBX_EXPORT PluginLoadedEvent final : public Event
    {
    public:
        PluginLoadedEvent(PluginMeta meta, WeakRef<Plugin> plugin)
            : _meta(std::move(meta))
            , _plugin(std::move(plugin)) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        const PluginMeta& GetMeta() const
        {
            return _meta;
        }

        WeakRef<Plugin> GetPlugin() const
        {
            return _plugin;
        }

    private:
        PluginMeta _meta = {};
        WeakRef<Plugin> _plugin = {};
    };

    class TBX_EXPORT PluginUnloadedEvent final : public Event
    {
    public:
        PluginUnloadedEvent(PluginMeta meta, WeakRef<Plugin> plugin)
            : _meta(std::move(meta))
            , _plugin(std::move(plugin)) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        const PluginMeta& GetMeta() const
        {
            return _meta;
        }

        WeakRef<Plugin> GetPlugin() const
        {
            return _plugin;
        }

    private:
        PluginMeta _meta = {};
        WeakRef<Plugin> _plugin = {};
    };

    class TBX_EXPORT PluginsUnloadedEvent final : public Event
    {
    public:
        PluginsUnloadedEvent(const std::vector<Ref<Plugin>>& unloadedPlugins)
            : _unloadedPlugins(unloadedPlugins) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        const std::vector<Ref<Plugin>>& GetUnloadedPlugins() const
        {
            return _unloadedPlugins;
        }

    private:
        std::vector<Ref<Plugin>> _unloadedPlugins = {};
    };

    class TBX_EXPORT PluginsLoadedEvent final : public Event
    {
    public:
        PluginsLoadedEvent(const std::vector<Ref<Plugin>>& loadedPlugins)
            : _loadedPlugins(loadedPlugins) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        const std::vector<Ref<Plugin>>& GetLoadedPlugins() const
        {
            return _loadedPlugins;
        }

    private:
        std::vector<Ref<Plugin>> _loadedPlugins = {};
    };
}
