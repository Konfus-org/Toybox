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
        EXPORT static void Initialize(const std::string& pathToPlugins);
        EXPORT static void Shutdown();

        EXPORT static void ReloadPlugins();
        EXPORT static void RegisterPlugin(const std::shared_ptr<LoadedPlugin>& plugin);

        /// <summary>
        /// Gets the first plugin of a type.
        /// </summary>
        template <typename T>
        EXPORT static std::weak_ptr<T> Get()
        {
            const auto& loadedPlugins = GetAll();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto plugImpl = loadedPlug->GetAs<T>();
                if (plugImpl.expired() || !plugImpl.lock()) continue;

                return plugImpl;
            }

            return {};
        }

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename T>
        EXPORT static std::vector<std::weak_ptr<T>> GetAllOfType()
        {
            std::vector<std::weak_ptr<T>> plugins;
            const auto& loadedPlugins = GetAll();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto& plugImpl = loadedPlug->GetAs<T>();
                if (plugImpl.expired() || !plugImpl.lock()) continue;

                plugins.push_back(plugImpl);
            }

            return plugins;
        }

        /// <summary>
        /// Gets all loaded plugins.
        /// </summary>
        EXPORT static const std::vector<std::shared_ptr<LoadedPlugin>>& GetAll();

    private:
        static std::vector<PluginInfo> FindPluginInfosInDirectory(const std::string& pathToPlugins);

        static std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins;
        static std::string _pathToLoadedPlugins;
    };
}

