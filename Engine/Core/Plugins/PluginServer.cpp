#include "TbxPCH.h"
#include "PluginServer.h"

namespace Tbx
{
    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::_loadedPlugins;

    ////std::weak_ptr<Plugin> PluginServer::GetPlugin(const std::string_view& name)
    ////{
    ////    for (const auto& loadedMod : _loadedPlugins)
    ////    {
    ////        const auto& plug = loadedMod->GetPlugin().lock();
    ////        if (plug->GetName() == name)
    ////        {
    ////            return plug;
    ////        }
    ////    }

    ////    return {};
    ////}

    std::vector<std::weak_ptr<Plugin>> PluginServer::GetPlugins()
    {
        std::vector<std::weak_ptr<Plugin>> plugins;
        for (const auto& loadedPlugin : _loadedPlugins)
        {
            const auto& plug = loadedPlugin->GetPlugin().lock();
            plugins.push_back(plug);
        }
        return plugins;
    }

    template<typename T>
    std::weak_ptr<FactoryPlugin<T>> PluginServer::GetPlugin()
    {
        for (const auto& loadedMod : _loadedPlugins)
        {
            const auto& mod = loadedMod->GetPlugin().lock();
            if (std::dynamic_pointer_cast<FactoryPlugin<T>>(mod))
            {
                return std::static_pointer_cast<FactoryPlugin<T>>(mod);
            }
        }

        return {};
    }

    void PluginServer::LoadPlugins(const std::string& pathToPlugins)
    {
        const auto& pluginsInPluginDir = std::filesystem::directory_iterator(pathToPlugins);
        for (const auto& entry : pluginsInPluginDir)
        {
            // Recursively load directories
            if (entry.is_directory()) LoadPlugins(entry.path().string());
            
            // Skip anything that isn't a file
            if (!entry.is_regular_file()) continue;

            // Platform-specific extension check
#if defined(TBX_PLATFORM_WINDOWS)
            if (entry.path().extension() == ".dll")
#elif defined(TBX_PLATFORM_LINUX)
            if (entry.path().extension() == ".so")
#elif defined(TBX_PLATFORM_OSX)
            if (entry.path().extension() == ".dylib")
#else
            if (false)
#endif
            {
                const std::string& location = entry.path().string();
                auto plug = std::make_shared<LoadedPlugin>(location);
                _loadedPlugins.push_back(plug);
            }
        }
    }

    void PluginServer::Shutdown()
    {
        _loadedPlugins.clear();
    }
}