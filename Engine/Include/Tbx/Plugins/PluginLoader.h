#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <string>
#include <utility>
#include <vector>

namespace Tbx
{
    class TBX_EXPORT PluginCollection
    {
    public:
        PluginCollection() = default;
        PluginCollection(std::vector<ExclusiveRef<LoadedPlugin>> plugins, Ref<EventBus> eventBus) noexcept;
        ~PluginCollection();

        PluginCollection(const PluginCollection&) = delete;
        PluginCollection& operator=(const PluginCollection&) = delete;
        PluginCollection(PluginCollection&&) noexcept = default;
        PluginCollection& operator=(PluginCollection&&) noexcept = default;

        bool Empty() const noexcept;
        uint32 Count() const noexcept;

        std::vector<Ref<IPlugin>> All() const;

        template <typename TPlugin>
        std::vector<Ref<TPlugin>> OfType() const
        {
            std::vector<Ref<TPlugin>> result;
            result.reserve(_plugins.size());

            for (const auto& record : _plugins)
            {
                if (record == nullptr)
                {
                    continue;
                }

                if (auto casted = record->GetAs<TPlugin>())
                {
                    result.push_back(std::move(casted));
                }
            }

            return result;
        }

        Ref<IPlugin> OfName(const std::string& pluginName) const;

        auto begin() const noexcept { return _plugins.begin(); }
        auto end() const noexcept { return _plugins.end(); }

    private:
        std::vector<ExclusiveRef<LoadedPlugin>> _plugins;
        Ref<EventBus> _eventBus = nullptr;
    };


    /// <summary>
    /// Responsible for loading plugins described by metadata and providing access to them.
    /// </summary>
    class TBX_EXPORT PluginLoader
    {
    public:
        PluginLoader(
            std::vector<PluginMeta> plugins,
            Ref<EventBus> eventBus);
        ~PluginLoader() = default;

        PluginLoader(const PluginLoader&) = delete;
        PluginLoader& operator=(const PluginLoader&) = delete;
        PluginLoader(PluginLoader&&) noexcept = default;
        PluginLoader& operator=(PluginLoader&&) noexcept = default;

        /// <summary>
        /// Produces the loaded plugin collection, transferring ownership to the caller.
        /// </summary>
        PluginCollection Results();

    private:
        void LoadPlugins(std::vector<PluginMeta> pluginMetas);

    private:
        std::vector<ExclusiveRef<LoadedPlugin>> _plugins;
        Ref<EventBus> _eventBus = nullptr;
    };
}
