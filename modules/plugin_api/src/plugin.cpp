#include "tbx/plugin_api/plugin.h"
#include "tbx/common/result.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"
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

    void Plugin::attach(IPluginHost& host)
    {
        _host = &host;
        _dispatcher = &host.get_dispatcher();
        on_attach(host);
    }

    void Plugin::detach()
    {
        on_detach();
        _dispatcher = nullptr;
        _host = nullptr;
    }

    void Plugin::update(const DeltaTime& dt)
    {
        on_update(dt);
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

    IPluginHost& Plugin::get_host() const
    {
        TBX_ASSERT(_host, "Plugins must be attached before accessing the host.");
        return *_host;
    }
}
