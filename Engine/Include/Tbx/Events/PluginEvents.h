#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Plugins/LoadedPlugin.h"

namespace Tbx
{
    class PluginLoadedEvent : public Event
    {
    public:
        EXPORT PluginLoadedEvent(std::shared_ptr<LoadedPlugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        EXPORT std::string ToString() const final
        {
            return "Plugin Loaded Event";
        }

        EXPORT std::shared_ptr<LoadedPlugin> GetLoadedPlugin() 
        {
            return _loadedPlugin; 
        }

    private:
        std::shared_ptr<LoadedPlugin> _loadedPlugin;
    };
}