#include "tbx/systems/plugin_api/loaded_plugin.h"
#include "tbx/systems/debugging/macros.h"

namespace tbx
{
    LoadedPlugin::LoadedPlugin(
        PluginMeta meta_data,
        std::unique_ptr<SharedLibrary> plugin_library,
        std::unique_ptr<Plugin, PluginDeleter> plugin_instance,
        RegisterPluginServicesFn register_services,
        BindPluginRuntimeFn bind_runtime)
        : meta(std::move(meta_data))
        , library(std::move(plugin_library))
        , instance(std::move(plugin_instance))
        , _register_services(register_services)
        , _bind_runtime(bind_runtime)
    {
        TBX_ASSERT(!meta.name.empty(), "LoadedPlugin requires a name.");
        TBX_ASSERT(!meta.version.empty(), "LoadedPlugin requires a version.");
        TBX_ASSERT(instance, "LoadedPlugin requires a plugin instance.");
    }

    LoadedPlugin::~LoadedPlugin() noexcept
    {
        if (_state == LoadedPluginState::ATTACHED)
        {
            if (auto service_provider = _attached_service_provider.lock())
                detach(*service_provider);
            else
                TBX_ASSERT(false, "Attached plugin destroyed after its service provider expired.");
        }
    }

    bool LoadedPlugin::is_valid() const
    {
        return instance != nullptr;
    }

    bool LoadedPlugin::is_attached() const
    {
        return _state == LoadedPluginState::ATTACHED;
    }

    void LoadedPlugin::attach(std::shared_ptr<ServiceProvider> service_provider)
    {
        if (!is_valid() || _state == LoadedPluginState::ATTACHED || !service_provider)
            return;

        TBX_ASSERT(
            _plugin_id.is_valid(),
            "LoadedPlugin must have a valid plugin id before attach.");

        TBX_TRACE_INFO("Loading plugin: {} v{}", meta.name, meta.version);
        _state = LoadedPluginState::ATTACHED;
        _attached_service_provider = service_provider;
        try
        {
            instance->attach(*service_provider, _plugin_id);
        }
        catch (...)
        {
            _state = LoadedPluginState::UNATTACHED;
            _attached_service_provider = {};
            throw;
        }
    }

    void LoadedPlugin::bind_runtime(ServiceProvider& service_provider)
    {
        if (!is_valid() || !_bind_runtime)
            return;

        _bind_runtime(instance.get(), &service_provider);
    }

    void LoadedPlugin::register_services(ServiceProvider& service_provider)
    {
        if (!is_valid() || _services_registered)
            return;

        if (_register_services)
        {
            auto plugin_scope = ScopedPluginContext(_plugin_id);
            _register_services(instance.get(), &service_provider);
        }
        _services_registered = true;
    }

    void LoadedPlugin::fixed_update(const DeltaTime& dt)
    {
        if (!is_valid() || !is_attached())
            return;

        instance->fixed_update(dt);
    }

    void LoadedPlugin::detach(ServiceProvider& service_provider)
    {
        if (!is_valid() || _state != LoadedPluginState::ATTACHED)
            return;

        TBX_TRACE_INFO("Unloading plugin: {}", meta.name);
        instance->detach(service_provider);
        _services_registered = false;
        _state = LoadedPluginState::DETACHED;
        _attached_service_provider = {};
    }

    void LoadedPlugin::receive_message(Message& msg)
    {
        if (!is_valid() || _state != LoadedPluginState::ATTACHED)
            return;

        instance->receive_message(msg);
    }

    void LoadedPlugin::update(const DeltaTime& dt)
    {
        if (!is_valid() || !is_attached())
            return;

        instance->update(dt);
    }

    void LoadedPlugin::set_id(Uuid plugin_id)
    {
        _plugin_id = plugin_id;
    }

    Uuid LoadedPlugin::get_id() const
    {
        return _plugin_id;
    }

}
