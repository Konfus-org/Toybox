#pragma once
#include "Tbx/App/Plugins/SharedLibrary.h"
#include "Tbx/App/Plugins/Plugin.h"
#include "Tbx/App/Plugins/PluginInfo.h"

namespace Tbx
{
    class LoadedPlugin
    {
    public:
        explicit(false) LoadedPlugin(const std::string& folderPath, const std::string& pluginDllFileName) { Load(folderPath, pluginDllFileName); }
        ~LoadedPlugin() { Unload(); }

        const PluginInfo& GetPluginInfo() const { return _pluginInfo; }
        std::shared_ptr<IPlugin> GetPlugin() const { return _plugin; }

    private:
        void Load(const std::string& folderPath, const std::string& pluginDllFileName);
        void Unload();

        PluginInfo _pluginInfo;
        SharedLibrary _library;
        std::shared_ptr<IPlugin> _plugin = nullptr;
    };
}
