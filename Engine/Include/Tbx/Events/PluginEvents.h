#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class PluginLoadedEvent : public Event
    {
    public:
        EXPORT PluginLoadedEvent(Ref<LoadedPlugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        EXPORT std::string ToString() const final
        {
            return "Plugin Loaded Event";
        }

        EXPORT Ref<LoadedPlugin> GetLoadedPlugin() 
        {
            return _loadedPlugin; 
        }

    private:
        Ref<LoadedPlugin> _loadedPlugin;
    };
}