#pragma once
#include <Tbx/Core/Plugins/PluginsAPI.h>
#include <Tbx/Runtime/Plugin Server/PluginInfo.h>
#include <Tbx/Core/DllExport.h>
#include <memory>

namespace Tbx
{
    class LoadedPlugin
    {
    public:
        EXPORT explicit(false) LoadedPlugin(const std::string& pluginFolderPath, const std::string& pluginFileName) 
        {
            Load(pluginFolderPath, pluginFileName);
        }

        EXPORT ~LoadedPlugin()
        { 
            Unload(); 
        }

        EXPORT bool IsValid() const { return _plugin != nullptr; }
        EXPORT const PluginInfo& GetPluginInfo() const { return _pluginInfo; }
        EXPORT std::shared_ptr<IPlugin> GetPlugin() const { return _plugin; }

    private:
        void Load(const std::string& pluginFolderPath, const std::string& pluginFileName);
        void Unload();

        PluginInfo _pluginInfo;
        SharedLibrary _library;
        std::shared_ptr<IPlugin> _plugin = nullptr;
    };
}
