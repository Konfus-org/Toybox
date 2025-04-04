#pragma once
#include "Tbx/Core/Plugins/LoadedPlugin.h"
#include "Tbx/Core/DllExport.h"
#include <memory>
#include <string>
#include <vector>

namespace Tbx
{
    class PluginServer
    {
    public:
        EXPORT static void LoadPlugins(const std::string& pathToPlugins);
        EXPORT static void ReloadPlugins(const std::string& pathToPlugins);
        EXPORT static void Shutdown();

        EXPORT static void RegisterPlugin(std::shared_ptr<LoadedPlugin> plugin);
        EXPORT static std::vector<std::shared_ptr<LoadedPlugin>> GetLoadedPlugins();

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
        static std::vector<PluginInfo> FindPluginInfosInDirectory(const std::string& pathToPlugins);

        static std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins;
    };
}

