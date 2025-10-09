#pragma once
#include "Tbx/Events/EventBus.h"
#include "Tbx/Collections/Queryable.h"
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Memory/Refs.h"
#include <string>
#include <vector>

namespace Tbx
{
    class TBX_EXPORT PluginContainer : public Queryable<Ref<Plugin>>
    {
    public:
        PluginContainer() = default;
        PluginContainer(const std::vector<Ref<Plugin>>& plugins);
        Ref<Plugin> OfName(const std::string& pluginName) const;
    };


    /// <summary>
    /// Responsible for loading plugins described by metadata and providing access to them.
    /// </summary>
    class TBX_EXPORT PluginLoader
    {
    public:
        PluginLoader(
            const std::vector<PluginMeta>& pluginMetas,
            Ref<EventBus> eventBus);

        /// <summary>
        /// Produces the loaded plugin collection, transferring ownership to the caller.
        /// </summary>
        PluginContainer Results();

    private:
        void LoadPlugins(const std::vector<PluginMeta>& pluginMetas);

    private:
        std::vector<Ref<Plugin>> _plugins;
        Ref<EventBus> _eventBus = nullptr;
    };
}
