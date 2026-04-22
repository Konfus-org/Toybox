#include "tbx/interfaces/plugin.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/utils/result.h"
#include <string>


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

    void Plugin::attach(ServiceProvider& service_provider)
    {
        _dispatcher = &service_provider.get_service<IMessageCoordinator>();
        on_attach(service_provider);
    }

    void Plugin::detach()
    {
        on_detach();
        _dispatcher = nullptr;
    }

    void Plugin::update(const DeltaTime& dt)
    {
        on_update(dt);
    }

    void Plugin::fixed_update(const DeltaTime& dt)
    {
        on_fixed_update(dt);
    }

    void Plugin::receive_message(Message& msg)
    {
        on_recieve_message(msg);
    }

    IMessageDispatcher& Plugin::get_dispatcher() const
    {
        TBX_ASSERT(_dispatcher, "Plugins must be attached before accessing the dispatcher.");
        return *_dispatcher;
    }

}
