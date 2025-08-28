#pragma once
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include "Tbx/PluginAPI/SharedLibrary.h"
#include "Tbx/PluginAPI/PluginInfo.h"
#include "Tbx/DllExport.h"
#include <memory>

namespace Tbx
{
    class LoadedPlugin
    {
    public:
        EXPORT explicit(true) LoadedPlugin(const PluginInfo& pluginInfo);
        EXPORT ~LoadedPlugin();

        EXPORT LoadedPlugin(const LoadedPlugin&) = delete;
        EXPORT LoadedPlugin& operator= (const LoadedPlugin&) = delete;

        EXPORT bool IsValid() const;
        EXPORT const PluginInfo& GetInfo() const;
        EXPORT void Reload();

        /// <summary>
        /// Attempts to get the loaded plugin as the requested type.
        /// If the plugin is not of the requested type, nullptr is returned.
        /// </summary>
        template <typename T>
        EXPORT std::weak_ptr<T> GetAs()
        {
            // Try to cast the plugin to the requested type
            if (const auto& castedPlug = std::dynamic_pointer_cast<T>(_plugin)) 
                return castedPlug;
            
            // Failed to cast, return empty
            return {};
        }


        /// <summary>
        /// Gets the loaded plugin.
        /// </summary>
        EXPORT std::weak_ptr<IPlugin> Get()
        {
            return _plugin;
        }

    private:
        void Load();
        void Unload();

        PluginInfo _pluginInfo = {};
        SharedLibrary _library = {};
        std::shared_ptr<IPlugin> _plugin = nullptr;
    };
}
