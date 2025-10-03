#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class Plugin;

    class TBX_EXPORT PluginLoadedEvent final : public Event
    {
    public:
        PluginLoadedEvent(Ref<Plugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        Ref<Plugin> GetLoadedPlugin() const
        {
            return _loadedPlugin;
        }

    private:
        Ref<Plugin> _loadedPlugin = nullptr;
    };

    class TBX_EXPORT PluginUnloadedEvent final : public Event
    {
    public:
        PluginUnloadedEvent(Ref<Plugin> unloadedPlugin)
            : _unloadedPlugin(unloadedPlugin) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        Ref<Plugin> GetUnloadedPlugin() const
        {
            return _unloadedPlugin;
        }

    private:
        Ref<Plugin> _unloadedPlugin = nullptr;
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
