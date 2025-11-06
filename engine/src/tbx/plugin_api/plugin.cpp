#include "tbx/plugin_api/plugin.h"
#include "tbx/application.h"
#include "tbx/state/result.h"

namespace tbx
{
    Result Plugin::send_message(Message& msg) const
    {
        if (!_dispatcher)
        {
            Result result;
            result.set_status(ResultStatus::Failed, "Plugin dispatcher is not available.");
            return result;
        }

        return _dispatcher->send(msg);
    }

    void Plugin::on_attach(const ApplicationContext& context)
    {
        _dispatcher = &context.instance->get_dispatcher();
    }
}
