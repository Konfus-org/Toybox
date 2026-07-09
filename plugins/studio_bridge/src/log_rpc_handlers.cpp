#include "log_rpc_handlers.h"
#include "log_ops.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_log_handlers(const RpcRegistrar& registrar)
    {
        registrar.add(
            Wire::EDITOR_LOG,
            [](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor: write its line into the engine's unified log. No
                // response.
                write_editor_log(params);
            });
        registrar.add(
            Wire::ENGINE_SET_LOG_COLORS,
            [](const tbx::Json& params, tbx::RpcResponder& r)
            {
                set_log_colors(params);
                r.result(tbx::Json::object());
            });
    }
}
