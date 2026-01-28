#include "tbx/plugin_api/plugin.h"
#include "tbx/app/application.h"
#include "tbx/common/result.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/plugin_api/plugin_registry.h"
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

    void Plugin::attach(Application& host)
    {
        const std::string name = PluginRegistry::get_instance().get_registered_name(this);
        if (!name.empty())
        {
            TBX_TRACE_INFO("Attaching plugin: {} to app {}", name, host.get_name());
        }
        else
        {
            TBX_TRACE_INFO("Attaching plugin instance to app {}", host.get_name());
        }

        _host = &host;
        _dispatcher = get_global_dispatcher();
        on_attach(host);
    }

    void Plugin::detach()
    {
        const std::string name = PluginRegistry::get_instance().get_registered_name(this);
        const std::string app_name = _host ? _host->get_name() : std::string("Unknown");
        if (!name.empty())
        {
            TBX_TRACE_INFO("Detaching plugin: {} from app {}", name, app_name);
        }
        else
        {
            TBX_TRACE_INFO("Detaching plugin instance from app {}", app_name);
        }

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
