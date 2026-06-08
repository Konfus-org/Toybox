#include "tbx/interfaces/plugin.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/utils/result.h"

namespace tbx
{
    Result Plugin::dispatcher_missing_result(std::string_view action)
    {
        Result result;
        result.flag_failure(
            std::string("Plugin cannot ") + std::string(action)
            + " because it is not attached to a dispatcher.");
        return result;
    }

    Plugin::Plugin() = default;
    Plugin::~Plugin() noexcept = default;

    void Plugin::attach(ServiceProvider& service_provider, PluginInstanceId plugin_id)
    {
        auto dispatcher = service_provider.get_service<IMessageCoordinator>().lock();
        TBX_ASSERT(dispatcher != nullptr, "Plugin attach requires IMessageCoordinator service.");
        if (!dispatcher)
            return;

        _plugin_id = plugin_id;
        _dispatcher = service_provider.get_service<IMessageCoordinator>();
        auto plugin_scope = ScopedPluginContext(_plugin_id);
        on_attach();
    }

    void Plugin::detach(ServiceProvider&)
    {
        auto plugin_scope = ScopedPluginContext(_plugin_id);
        on_detach();
        _dispatcher = {};
        _plugin_id = PluginInstanceId{};
    }

    void Plugin::update(const DeltaTime& dt)
    {
        auto plugin_scope = ScopedPluginContext(_plugin_id);
        on_update(dt);
    }

    void Plugin::fixed_update(const DeltaTime& dt)
    {
        auto plugin_scope = ScopedPluginContext(_plugin_id);
        on_fixed_update(dt);
    }

    void Plugin::receive_message(Message& msg)
    {
        auto plugin_scope = ScopedPluginContext(_plugin_id);
        on_recieve_message(msg);
    }

    IMessageDispatcher& Plugin::get_dispatcher() const
    {
        const auto dispatcher = _dispatcher.lock();
        TBX_ASSERT(
            dispatcher != nullptr,
            "Plugins must be attached before accessing the dispatcher.");
        return *dispatcher;
    }

    Uuid Plugin::get_id() const
    {
        return _plugin_id;
    }

}
