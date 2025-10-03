#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Tbx
{
    struct PluginCollectionData
    {
        std::vector<Ref<Plugin>> Plugins;
        Ref<EventBus> EventBus = nullptr;
    };

    class TBX_EXPORT PluginCollection
    {
    public:
        PluginCollection() = default;
        PluginCollection(std::vector<Ref<Plugin>> plugins, Ref<EventBus> eventBus);
        ~PluginCollection();

        PluginCollection(const PluginCollection&) = default;
        PluginCollection& operator=(const PluginCollection&) = default;
        PluginCollection(PluginCollection&&) = default;
        PluginCollection& operator=(PluginCollection&&) = default;

        bool Empty() const;
        uint32 Count() const;

        std::vector<Ref<Plugin>> All() const;

        template <typename TPlugin>
        std::vector<Ref<TPlugin>> OfType() const
        {
            std::vector<Ref<TPlugin>> result;
            const auto& plugins = Items();
            result.reserve(plugins.size());

            for (const auto& plugin : plugins)
            {
                if (plugin == nullptr)
                {
                    continue;
                }

                if (auto casted = std::dynamic_pointer_cast<TPlugin>(plugin))
                {
                    result.push_back(std::move(casted));
                }
            }

            return result;
        }

        Ref<Plugin> OfName(const std::string& pluginName) const;

        auto begin() const { return Items().begin(); }
        auto end() const { return Items().end(); }

    private:
        const std::vector<Ref<Plugin>>& Items() const;

        std::shared_ptr<PluginCollectionData> _data = nullptr;
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
        std::vector<Ref<Plugin>> _plugins;
        Ref<EventBus> _eventBus = nullptr;
    };
}
