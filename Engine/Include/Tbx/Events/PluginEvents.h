#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class TBX_EXPORT PluginLoadedEvent final : public Event
    {
    public:
        PluginLoadedEvent(Ref<Plugin> loadedPlugin)
            : _loadedPlugin(loadedPlugin) {}

        std::string ToString() const override
        {
            return "Plugin Loaded Event";
        }

        Ref<Plugin> GetLoadedPlugin() 
        {
            return _loadedPlugin; 
        }

    private:
        Ref<Plugin> _loadedPlugin;
    };
}