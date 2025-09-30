#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class TBX_EXPORT PluginLoadedEvent final : public Event
    {
    public:
        PluginLoadedEvent(Ref<IPlugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        Ref<IPlugin> GetLoadedPlugin() 
        {
            return _loadedPlugin; 
        }

    private:
        Ref<IPlugin> _loadedPlugin;
    };

    class TBX_EXPORT PluginUnloadedEvent final : public Event
    {
    public:
        PluginUnloadedEvent(Ref<IPlugin> unloadedPlugin)
            : _unloadedPlugin(unloadedPlugin) {
        }

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        Ref<IPlugin> GetUnloadedPlugin()
        {
            return _unloadedPlugin;
        }

    private:
        Ref<IPlugin> _unloadedPlugin;
    };
}