#pragma once
#include "Tbx/Utils/DllExport.h"
#include "Tbx/Systems/Plugins/LoadedPlugin.h"
#include <memory>
#include <string>
#include <vector>

namespace Tbx
{
    class PluginServer
    {
    public:
        EXPORT static void LoadPlugins(const std::string& pathToPlugins);
        EXPORT static void ReloadPlugins();
        EXPORT static void RestartPlugins();
        EXPORT static void UnloadPlugins();

        EXPORT static void RegisterPlugin(const std::shared_ptr<LoadedPlugin>& plugin);

        template <typename T>
        EXPORT static std::shared_ptr<T> GetPlugin()
        {
            const auto& loadedPlugins = GetLoadedPlugins();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto& plugImpl = loadedPlug->GetAs<T>();
                if (!plugImpl) continue;

                return plugImpl;
            }

            return nullptr;
        }

        template <typename T>
        EXPORT static std::vector<std::shared_ptr<T>> GetPlugins()
        {
            std::vector<std::shared_ptr<T>> plugins;
            const auto& loadedPlugins = GetLoadedPlugins();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto& plugImpl = loadedPlug->GetAs<T>();
                if (!plugImpl) continue;

                plugins.push_back(plugImpl);
            }

            return plugins;
        }

    private:
        EXPORT static std::vector<std::shared_ptr<LoadedPlugin>> GetLoadedPlugins();
        static std::vector<PluginInfo> FindPluginInfosInDirectory(const std::string& pathToPlugins);

        static std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins;
        static std::string _pathToLoadedPlugins;
    };
}

