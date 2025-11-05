#include "tbx/plugin_api/plugin.h"
#include "tbx/application.h"
#include "tbx/state/result.h"

namespace tbx
{
    Result Plugin::send_message(const Message& msg) const
    {
        if (!_dispatcher)
        {
            Result result;
            result.set_status(ResultStatus::Failed, "Plugin dispatcher is not available.");
            return result;
        }

        return _dispatcher->send(msg);
    }

    IMessageDispatcher* Plugin::dispatcher() const noexcept
    {
        return _dispatcher;
    }

    void Plugin::set_host(Application* application) noexcept
    {
        _dispatcher = application ? &application->get_dispatcher() : nullptr;
    }

    StaticPluginRegistration::StaticPluginRegistration(
        const char* entry_point,
        CreatePluginFn create,
        DestroyPluginFn destroy)
        : _entry_point(entry_point ? entry_point : "")
    {
        if (_entry_point.empty())
        {
            return;
        }

        PluginRegistry::instance().register_static_plugin_entry(_entry_point, create, destroy);
    }

    StaticPluginRegistration::~StaticPluginRegistration()
    {
        if (_entry_point.empty())
        {
            return;
        }

        PluginRegistry::instance().unregister_static_plugin_entry(_entry_point);
    }
}
