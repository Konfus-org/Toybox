#pragma once
#include "Tbx/Events/EventBus.h"
#include "Tbx/Collections/Queryable.h"
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Memory/Refs.h"
#include <string>
#include <utility>
#include <vector>

namespace Tbx
{
    class TBX_EXPORT PluginCache : public Queryable<Ref<Plugin>>
    {
    public:
        PluginCache() = default;
        explicit PluginCache(std::vector<Ref<Plugin>> plugins);
        ~PluginCache();

        PluginCache(const PluginCache&) = delete;
        PluginCache& operator=(const PluginCache&) = delete;
        PluginCache(PluginCache&&) noexcept = default;
        PluginCache& operator=(PluginCache&&) noexcept = default;

        Ref<Plugin> OfName(const std::string& pluginName) const;
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
        PluginCache Results();

    private:
        void LoadPlugins(std::vector<PluginMeta> pluginMetas);

    private:
        std::vector<Ref<Plugin>> _plugins;
        Ref<EventBus> _eventBus = nullptr;
    };
}
