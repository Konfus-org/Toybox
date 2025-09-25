#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <string>
#include <vector>

namespace Tbx
{
    class TBX_EXPORT PluginServer
    {
    public:
        PluginServer(
            const std::string& pathToPlugins,
            Ref<EventBus> eventBus,
            WeakRef<Tbx::App> app);
        ~PluginServer();

        /// <summary>
        /// Registers a plugin
        /// </summary>
        /// <param name="plugin"></param>
        void AddPlugin(const Ref<LoadedPlugin>& plugin);

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename TPlugin>
        std::vector<Ref<TPlugin>> GetPlugins()
        {
            auto plugins = std::vector<Ref<TPlugin>>();
            const auto& loadedPlugins = GetPlugins();

            for (const auto& loadedPlug : loadedPlugins)
            {
                Ref<TPlugin> plugImpl = loadedPlug->GetAs<TPlugin>();
                if (!plugImpl) continue;
                plugins.push_back(plugImpl);
            }

            return plugins;
        }

        /// <summary>
        /// Gets all loaded plugins.
        /// </summary>
        const std::vector<Ref<LoadedPlugin>>& GetPlugins();

    private:
        std::vector<PluginMeta> SearchDirectoryForInfos(const std::string& pathToPlugins);
        void LoadPlugins(const std::string& pathToPlugins, std::weak_ptr<Tbx::App> app);
        void UnloadPlugins();

    private:
        std::string _pathToLoadedPlugins = "";
        std::vector<Ref<LoadedPlugin>> _loadedPlugins = {};
        Ref<EventBus> _eventBus = nullptr;
    };
}

