#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Events/EventBus.h"
#include <memory>
#include <string>
#include <vector>

namespace Tbx
{
    class PluginManager
    {
    public:
        PluginManager(const std::string& pathToPlugins, std::shared_ptr<EventBus> eventBus, const std::weak_ptr<Tbx::App>& app);
        ~PluginManager();

        /// <summary>
        /// Registers a plugin
        /// </summary>
        /// <param name="plugin"></param>
        EXPORT void AddPlugin(const std::shared_ptr<LoadedPlugin>& plugin);

        /// <summary>
        /// Gets the first plugin of a type.
        /// </summary>
        template <typename TPlugin>
        EXPORT std::shared_ptr<TPlugin> GetPlugin()
        {
            const auto& loadedPlugins = GetPlugins();

            for (const auto& loadedPlug : loadedPlugins)
            {
                std::shared_ptr<TPlugin> plugImpl = loadedPlug->GetAs<TPlugin>();
                if (!plugImpl) continue;
                return plugImpl;
            }

            return {};
        }

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename TPlugin>
        EXPORT std::vector<std::shared_ptr<TPlugin>> GetPlugins()
        {
            auto plugins = std::vector<std::shared_ptr<TPlugin>>();
            const auto& loadedPlugins = GetPlugins();

            for (const auto& loadedPlug : loadedPlugins)
            {
                std::shared_ptr<TPlugin> plugImpl = loadedPlug->GetAs<TPlugin>();
                if (!plugImpl) continue;
                plugins.push_back(plugImpl);
            }

            return plugins;
        }

        /// <summary>
        /// Gets all loaded plugins.
        /// </summary>
        EXPORT const std::vector<std::shared_ptr<LoadedPlugin>>& GetPlugins();

    private:
        std::vector<PluginMeta> SearchDirectoryForInfos(const std::string& pathToPlugins);
        EXPORT void LoadPlugins(const std::string& pathToPlugins);
        EXPORT void UnloadPlugins();

        std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins = {};
        std::string _pathToLoadedPlugins = "";

        std::shared_ptr<EventBus> _eventBus = nullptr;
    };
}

