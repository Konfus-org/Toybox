#pragma once
#include "Tbx/Systems/Plugins/IPlugin.h"
#include "Tbx/Systems/Plugins/SharedLibrary.h"
#include "Tbx/Systems/Plugins/PluginInfo.h"
#include "Tbx/Utils/DllExport.h"
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
        EXPORT void Restart() const;

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
            
            // Failed to cast, return nullptr
            return nullptr;
        }

    private:
        void Load();
        void Unload();

        PluginInfo _pluginInfo = {};
        SharedLibrary _library = {};
        std::shared_ptr<IPlugin> _plugin = nullptr;
    };
}
