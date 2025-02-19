#include "TbxPCH.h"
#include "PluginServer.h"
#include "Util/SmartPointerUtils.h"
#include <filesystem>

namespace Tbx
{
    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::_loadedPlugins;

    std::shared_ptr<IPlugin> PluginServer::GetPlugin(const std::string_view& name)
    {
        for (const auto& loadedPlug : _loadedPlugins)
        {
            if (loadedPlug->GetPluginInfo().GetName() == name)
            {
                return loadedPlug->GetPlugin();
            }
        }

        return nullptr;
    }

    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::GetLoadedPlugins()
    {
        return _loadedPlugins;
    }

    template<typename T>
    std::shared_ptr<T> PluginServer::GetPlugin()
    {
        for (const auto& loadedPlug : _loadedPlugins)
        {
            const auto& plug = loadedPlug->GetPlugin();
            const auto& castedPlug = std::dynamic_pointer_cast<Plugin<T>>(plug);
            if (castedPlug)
            {
                return castedPlug->GetImplementation();
            }
        }

        return nullptr;
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
                const std::string& fileName = entry.path().filename().string();
                auto plug = std::make_shared<LoadedPlugin>(pathToPlugins, fileName);
                _loadedPlugins.push_back(plug);
            }
        }
    }

    void PluginServer::Shutdown()
    {
        // Clear refs to loaded plugins.. 
        // this will cause them to unload themselves when all refs to them die
        _loadedPlugins.clear();
    }
}