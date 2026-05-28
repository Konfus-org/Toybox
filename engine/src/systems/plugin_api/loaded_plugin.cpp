#include "tbx/systems/plugin_api/loaded_plugin.h"
#include "tbx/systems/debugging/macros.h"

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
        if (_state == LoadedPluginState::ATTACHED && _attached_service_provider != nullptr)
        {
            detach(*_attached_service_provider);
        }
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
        _attached_service_provider = &service_provider;
        try
        {
            instance->attach(service_provider);
        }
        catch (...)
        {
            _state = LoadedPluginState::UNATTACHED;
            _attached_service_provider = nullptr;
            throw;
        }
    }

    void LoadedPlugin::detach(ServiceProvider& service_provider)
    {
        if (!is_valid() || _state != LoadedPluginState::ATTACHED)
            return;

        TBX_TRACE_INFO("Unloading plugin: {}", meta.name);
        instance->detach(service_provider);
        _state = LoadedPluginState::DETACHED;
        _attached_service_provider = nullptr;
    }

    void LoadedPlugin::receive_message(Message& msg)
    {
        if (!is_valid() || _state != LoadedPluginState::ATTACHED)
            return;

        instance->receive_message(msg);
    }

}
