#pragma once
#include "Tbx/PluginAPI/LoadedPlugin.h"
#include "Tbx/DllExport.h"
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
        EXPORT static void UnloadPlugins();

        EXPORT static void RegisterPlugin(const std::shared_ptr<LoadedPlugin>& plugin);

        /// <summary>
        /// Gets the first plugin of a type.
        /// </summary>
        template <typename T>
        EXPORT static std::shared_ptr<T> GetPlugin()
        {
            const auto& loadedPlugins = GetAllPlugins();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto& plugImpl = loadedPlug->GetAs<T>();
                if (!plugImpl) continue;

                return plugImpl;
            }

            return nullptr;
        }

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename T>
        EXPORT static std::vector<std::shared_ptr<T>> GetPlugins()
        {
            std::vector<std::shared_ptr<T>> plugins;
            const auto& loadedPlugins = GetAllPlugins();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto& plugImpl = loadedPlug->GetAs<T>();
                if (!plugImpl) continue;

                plugins.push_back(plugImpl);
            }

            return plugins;
        }

        /// <summary>
        /// Gets all loaded plugins.
        /// </summary>
        EXPORT static std::vector<std::shared_ptr<LoadedPlugin>> GetAllPlugins();

    private:
        static std::vector<PluginInfo> FindPluginInfosInDirectory(const std::string& pathToPlugins);

        static std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins;
        static std::string _pathToLoadedPlugins;
    };
}

