#include "tbx/plugin_api/plugin.h"
#include "tbx/common/string.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/result.h"

namespace tbx
{
    Result Plugin::dispatcher_missing_result(const String& action)
    {
        Result result;
        result.flag_failure(
            String("Plugin cannot ") + String(action)
            + " because it is not attached to a dispatcher.");
        return result;
    }

    Plugin::Plugin() = default;
    Plugin::~Plugin() = default;

    void Plugin::attach(Application& host)
    {
        _host = &host;
        _dispatcher = get_global_dispatcher();
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

    Application& Plugin::get_host() const
    {
        TBX_ASSERT(_host, "Plugins must be attached before accessing the host.");
        return *_host;
    }
}
