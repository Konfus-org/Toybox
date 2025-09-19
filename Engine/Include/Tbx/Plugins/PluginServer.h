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
    class PluginServer
    {
    public:
        PluginServer(
            const std::string& pathToPlugins,
            Tbx::Ref<EventBus> eventBus,
            Tbx::WeakRef<Tbx::App> app);
        ~PluginServer();

        /// <summary>
        /// Registers a plugin
        /// </summary>
        /// <param name="plugin"></param>
        EXPORT void AddPlugin(const Tbx::Ref<LoadedPlugin>& plugin);

        /// <summary>
        /// Gets the first plugin of a type.
        /// </summary>
        template <typename TPlugin>
        EXPORT Tbx::Ref<TPlugin> GetPlugin()
        {
            const auto& loadedPlugins = GetPlugins();

            for (const auto& loadedPlug : loadedPlugins)
            {
                Tbx::Ref<TPlugin> plugImpl = loadedPlug->GetAs<TPlugin>();
                if (!plugImpl) continue;
                return plugImpl;
            }

            return {};
        }

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename TPlugin>
        EXPORT std::vector<Tbx::Ref<TPlugin>> GetPlugins()
        {
            auto plugins = std::vector<Tbx::Ref<TPlugin>>();
            const auto& loadedPlugins = GetPlugins();

            for (const auto& loadedPlug : loadedPlugins)
            {
                Tbx::Ref<TPlugin> plugImpl = loadedPlug->GetAs<TPlugin>();
                if (!plugImpl) continue;
                plugins.push_back(plugImpl);
            }

            return plugins;
        }

        /// <summary>
        /// Gets all loaded plugins.
        /// </summary>
        EXPORT const std::vector<Tbx::Ref<LoadedPlugin>>& GetPlugins();

    private:
        std::vector<PluginMeta> SearchDirectoryForInfos(const std::string& pathToPlugins);
        EXPORT void LoadPlugins(const std::string& pathToPlugins, std::weak_ptr<Tbx::App> app);
        EXPORT void UnloadPlugins();

    private:
        std::string _pathToLoadedPlugins = "";
        std::vector<Tbx::Ref<LoadedPlugin>> _loadedPlugins = {};
        Tbx::Ref<EventBus> _eventBus = nullptr;
    };
}

