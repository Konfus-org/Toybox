#pragma once
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Plugins/PluginMeta.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/DllExport.h"
#include <memory>

namespace Tbx
{
    class LoadedPlugin
    {
    public:
        EXPORT explicit(true) LoadedPlugin(const PluginMeta& pluginInfo);
        EXPORT ~LoadedPlugin();

        EXPORT LoadedPlugin(const LoadedPlugin&) = delete;
        EXPORT LoadedPlugin& operator= (const LoadedPlugin&) = delete;

        EXPORT void Reload();
        EXPORT bool IsValid() const;
        EXPORT const PluginMeta& GetMeta() const;

        /// <summary>
        /// Attempts to get the loaded plugin as the requested type.
        /// If the plugin is not of the requested type, nullptr is returned.
        /// </summary>
        template <typename T>
        EXPORT std::shared_ptr<T> GetAs()
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
        EXPORT std::shared_ptr<Plugin> Get()
        {
            return _plugin;
        }

    private:
        void Load();
        void Unload();

        PluginMeta _pluginInfo = {};
        SharedLibrary _library = {};
        std::shared_ptr<Plugin> _plugin = nullptr;
    };
}
