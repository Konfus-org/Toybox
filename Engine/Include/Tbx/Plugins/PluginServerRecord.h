#pragma once
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Plugins/PluginMeta.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class App;

    class TBX_EXPORT PluginServerRecord
    {
    public:
        explicit(true) PluginServerRecord(const PluginMeta& pluginInfo, Ref<EventBus> eventBus);
        ~PluginServerRecord();

        bool IsValid() const;
        const PluginMeta& GetMeta() const;

        /// <summary>
        /// Attempts to get the loaded plugin as the requested type.
        /// If the plugin is not of the requested type, nullptr is returned.
        /// </summary>
        template <typename T>
        Ref<T> GetAs() const
        {
            // Try to cast the plugin to the requested type
            if (const auto& castedPlug = std::dynamic_pointer_cast<T>(_plugin)) 
                return castedPlug;
            
            // Failed to cast, return empty
            return nullptr;
        }

        /// <summary>
        /// Gets the loaded plugin.
        /// </summary>
        Ref<IPlugin> Get() const
        {
            return _plugin;
        }

    private:
        void Load(Ref<EventBus> eventBus);
        void Unload();

        PluginMeta _pluginInfo = {};
        SharedLibrary _library = {};
        Ref<IPlugin> _plugin = nullptr;
    };
}
