#pragma once
#include "Tbx/Core/Plugins/Plugin.h"
#include "Tbx/Core/Plugins/SharedLibrary.h"
#include "Tbx/Core/Plugins/PluginInfo.h"
#include "Tbx/Core/DllExport.h"
#include <memory>
#include <string>

namespace Tbx
{
    class LoadedPlugin
    {
    public:
        EXPORT LoadedPlugin(const std::string& pluginFolderPath, const std::string& pluginFileName);
        EXPORT ~LoadedPlugin();

        EXPORT LoadedPlugin(const LoadedPlugin&) = delete;
        EXPORT LoadedPlugin& operator= (const LoadedPlugin&) = delete;

        EXPORT bool IsValid() const;
        EXPORT const PluginInfo& GetPluginInfo() const;
        EXPORT std::shared_ptr<IPlugin> GetPlugin() const;

    private:
        void Load(const std::string& pluginFolderPath, const std::string& pluginFileName);
        void Unload();

        PluginInfo _pluginInfo;
        SharedLibrary _library;
        std::shared_ptr<IPlugin> _plugin = nullptr;
    };
}
