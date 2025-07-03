#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Plugin API/LoadedPlugin.h"

namespace Tbx
{
    class EXPORT PluginEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return static_cast<int>(EventCategory::Plugin);
        }
    };

    class PluginLoadedEvent : public PluginEvent
    {
    public:
        EXPORT PluginLoadedEvent(std::shared_ptr<LoadedPlugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        EXPORT std::string ToString() const override
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