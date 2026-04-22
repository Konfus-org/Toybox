#include "tbx/core/systems/plugin_api/loaded_plugin.h"
#include "tbx/core/systems/debugging/macros.h"
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
    }

    LoadedPlugin::~LoadedPlugin() noexcept
    {
        detach();
    }

    bool LoadedPlugin::is_valid() const
    {
        return instance != nullptr;
    }

    void LoadedPlugin::attach(ServiceProvider& service_provider)
    {
        if (!is_valid() || _state == LoadedPluginState::ATTACHED)
            return;

        TBX_TRACE_INFO("Loading plugin: {} v{}", meta.name, meta.version);
        _state = LoadedPluginState::ATTACHED;
        try
        {
            instance->attach(service_provider);
        }
        catch (...)
        {
            _state = LoadedPluginState::UNATTACHED;
            throw;
        }
    }

    void LoadedPlugin::detach()
    {
        if (!is_valid() || _state != LoadedPluginState::ATTACHED)
            return;

        TBX_TRACE_INFO("Unloading plugin: {}", meta.name);
        instance->detach();
        _state = LoadedPluginState::DETACHED;
    }

    void LoadedPlugin::receive_message(Message& msg)
    {
        if (!is_valid() || _state != LoadedPluginState::ATTACHED)
            return;

        instance->receive_message(msg);
    }

    std::string to_string(const LoadedPlugin& loaded)
    {
        return "Name=" + loaded.meta.name + ", Version=" + loaded.meta.version;
    }
}
