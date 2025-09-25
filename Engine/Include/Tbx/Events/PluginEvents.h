#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class TBX_EXPORT PluginLoadedEvent : public Event
    {
    public:
        PluginLoadedEvent(Ref<LoadedPlugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        std::string ToString() const final
        {
            return "Plugin Loaded Event";
        }

        Ref<LoadedPlugin> GetLoadedPlugin() 
        {
            return _loadedPlugin; 
        }

    private:
        Ref<LoadedPlugin> _loadedPlugin;
    };
}