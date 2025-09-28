#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/PluginServerRecord.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <string>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// The PluginServer is responsible for loading, keeping track of, and unloading plugins, as well as providing a way to access them.
    /// </summary>
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
        void RegisterPlugin(ExclusiveRef<PluginServerRecord> plugin);

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename TPlugin>
        std::vector<Ref<TPlugin>> GetPlugins() const
        {
            std::vector<Ref<TPlugin>> result;
            result.reserve(_pluginRecords.size());

            for (const auto& owned : _pluginRecords)
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

        void RemoveBackPlugin(std::vector<Tbx::ExclusiveRef<Tbx::PluginServerRecord>>& nonLoggerPlugs);

    private:
        std::string _pathToLoadedPlugins = "";
        std::vector<ExclusiveRef<PluginServerRecord>> _pluginRecords = {};
        Ref<EventBus> _eventBus = nullptr;
    };
}

