#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/debugging/macros.h"
#include <string>
#include <utility>

namespace tbx
{
    LoadedPlugin::LoadedPlugin(
        PluginMeta meta_data,
        std::unique_ptr<SharedLibrary> plugin_library,
        std::unique_ptr<Plugin, PluginDeleter> plugin_instance)
        : meta(std::move(meta_data))
        , library(std::move(plugin_library))
        , instance(std::move(plugin_instance))
    {
        TBX_ASSERT(!meta.name.empty(), "LoadedPlugin requires a name.");
        TBX_ASSERT(!meta.version.empty(), "LoadedPlugin requires a version.");
        TBX_ASSERT(instance, "LoadedPlugin requires a plugin instance.");
        TBX_TRACE_INFO("Loaded plugin: {} v{}", meta.name, meta.version);
    }

    LoadedPlugin::~LoadedPlugin() noexcept
    {
        if (!is_valid())
        {
            return;
        }

        TBX_TRACE_INFO("Unloading plugin: {} v{}", meta.name, meta.version);
    }

    bool LoadedPlugin::is_valid() const
    {
        return instance != nullptr;
    }

    std::string to_string(const LoadedPlugin& loaded)
    {
        return "Name=" + loaded.meta.name + ", Version=" + loaded.meta.version;
    }
}
