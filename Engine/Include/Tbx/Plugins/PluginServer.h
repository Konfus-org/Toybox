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
    struct PluginStack
    {
        PluginStack() = default;
        PluginStack(const std::vector<Ref<IPlugin>>& plugins)
            : _plugins(plugins) {}

        std::vector<Ref<IPlugin>> All() const { return _plugins; }
        void Push(Ref<IPlugin> plugin) { _plugins.push_back(plugin); }
        void Clear() { _plugins.clear(); }
        uint32 Count() const { return _plugins.size(); }

        /// <summary>
        /// Gets plugins of the specified type.
        /// </summary>
        template <typename TPlugin>
        std::vector<Ref<TPlugin>> OfType() const
        {
            std::vector<Ref<TPlugin>> result;
            result.reserve(_plugins.size());

            for (const auto& plug : _plugins)
            {
                if (auto casted = std::dynamic_pointer_cast<TPlugin>(plug))
                {
                    result.push_back(casted);
                }
            }

            return result;
        }

        auto begin() { return _plugins.begin(); }
        auto end() { return _plugins.end(); }
        auto begin() const { return _plugins.begin(); }
        auto end() const { return _plugins.end(); }

        const IPlugin& operator[](int index) const { return *_plugins[index]; }

    private:
        std::vector<Ref<IPlugin>> _plugins = {};
    };


    /// <summary>
    /// The PluginServer is responsible for loading, keeping track of, and unloading plugins, as well as providing a way to access them.
    /// </summary>
    class TBX_EXPORT PluginServer
    {
    public:
        PluginServer(
            const std::string& pathToPlugins,
            Ref<EventBus> eventBus);
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
        /// Gets all loaded plugins.
        /// </summary>
        PluginStack GetPlugins() const;

    private:
        std::vector<PluginMeta> SearchDirectoryForInfos(const std::string& pathToPlugins);
        void LoadPlugins(const std::string& pathToPlugins);
        void UnloadPlugins();

        void RemoveBackPlugin(std::vector<Tbx::ExclusiveRef<Tbx::PluginServerRecord>>& nonLoggerPlugs);

    private:
        std::string _pathToLoadedPlugins = "";
        std::vector<ExclusiveRef<PluginServerRecord>> _pluginRecords = {};
        Ref<EventBus> _eventBus = nullptr;
    };
}

