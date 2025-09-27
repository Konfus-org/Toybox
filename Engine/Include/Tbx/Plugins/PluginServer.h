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

        PluginServer(const PluginServer&) = delete;
        PluginServer& operator=(const PluginServer&) = delete;
        PluginServer(PluginServer&&) noexcept = default;
        PluginServer& operator=(PluginServer&&) noexcept = default;
        
        /// <summary>
        /// Registers a plugin
        /// </summary>
        /// <param name="plugin"></param>
        void RegisterPlugin(ExclusiveRef<LoadedPlugin> plugin);

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename TPlugin>
        std::vector<Ref<TPlugin>> GetPlugins() const
        {
            std::vector<Ref<TPlugin>> result;
            result.reserve(_loadedPlugins.size());

            for (const auto& owned : _loadedPlugins)
            {
                Ref<Plugin> base = owned->Get();
                if (!base) continue;

                if (auto casted = std::dynamic_pointer_cast<TPlugin>(base))
                {
                    result.push_back(casted);
                }
            }

            return result;
        }

        /// <summary>
        /// Gets all loaded plugins.
        /// </summary>
        std::vector<Ref<Plugin>> GetPlugins() const;

    private:
        std::vector<PluginMeta> SearchDirectoryForInfos(const std::string& pathToPlugins);
        void LoadPlugins(const std::string& pathToPlugins, std::weak_ptr<Tbx::App> app);
        void UnloadPlugins();

    private:
        std::string _pathToLoadedPlugins = "";
        std::vector<ExclusiveRef<LoadedPlugin>> _loadedPlugins = {};
        Ref<EventBus> _eventBus = nullptr;
    };
}

