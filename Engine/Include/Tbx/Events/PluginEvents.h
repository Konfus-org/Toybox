#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class IPlugin;

    class TBX_EXPORT PluginLoadedEvent final : public Event
    {
    public:
        PluginLoadedEvent(Ref<IPlugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        Ref<IPlugin> GetLoadedPlugin() const
        {
            return _loadedPlugin; 
        }

    private:
        Ref<IPlugin> _loadedPlugin = nullptr;
    };

    class TBX_EXPORT PluginUnloadedEvent final : public Event
    {
    public:
        PluginUnloadedEvent(Ref<IPlugin> unloadedPlugin)
            : _unloadedPlugin(unloadedPlugin) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        Ref<IPlugin> GetUnloadedPlugin() const
        {
            return _unloadedPlugin;
        }

    private:
        Ref<IPlugin> _unloadedPlugin = nullptr;
    };

    class TBX_EXPORT PluginsUnloadedEvent final : public Event
    {
    public:
        PluginsUnloadedEvent(const std::vector<Ref<IPlugin>>& unloadedPlugins)
            : _unloadedPlugins(unloadedPlugins) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        const std::vector<Ref<IPlugin>>& GetUnloadedPlugins() const
        {
            return _unloadedPlugins;
        }

    private:
        std::vector<Ref<IPlugin>> _unloadedPlugins = {};
    };

    class TBX_EXPORT PluginsLoadedEvent final : public Event
    {
    public:
        PluginsLoadedEvent(const std::vector<Ref<IPlugin>>& loadedPlugins)
            : _loadedPlugins(loadedPlugins) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        const std::vector<Ref<IPlugin>>& GetLoadedPlugins() const
        {
            return _loadedPlugins;
        }

    private:
        std::vector<Ref<IPlugin>> _loadedPlugins = {};
    };
}
