#pragma once
#include "Tbx/Events/EventBus.h"
#include "Tbx/Collections/Collection.h"
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Memory/Refs.h"
#include <string>
#include <vector>

namespace Tbx
{
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
        Collection<Ref<Plugin>> Results();

    private:
        void LoadPlugins(const std::vector<PluginMeta>& pluginMetas);

    private:
        std::vector<Ref<Plugin>> _plugins;
        Ref<EventBus> _eventBus = nullptr;
    };
}
