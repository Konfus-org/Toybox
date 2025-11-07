#include "tbx/plugin_api/plugin.h"
#include "tbx/debug/macros.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/state/result.h"
#include <string>
#include <string_view>

namespace tbx
{
    static Result dispatcher_missing_result(std::string_view action)
    {
        Result result;
        result.set_success(
            false,
            "Plugin cannot " + std::string(action)
                + " because it is not attached to a dispatcher.");
        return result;
    }

    Plugin::Plugin() = default;
    Plugin::~Plugin() = default;

    void Plugin::attach(Application& host)
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
        on_message(msg);
    }

    Result Plugin::send_message(Message& msg) const
    {
        if (!_dispatcher)
        {
            return dispatcher_missing_result("send a message");
        }

        return _dispatcher->send(msg);
    }

    Result Plugin::post_message(Message& msg) const
    {
        if (!_dispatcher)
        {
            return dispatcher_missing_result("post a message");
        }

        return _dispatcher->post(msg);
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
