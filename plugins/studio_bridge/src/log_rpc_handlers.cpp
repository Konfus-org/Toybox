#include "log_rpc_handlers.h"
#include "log_bridge.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_log_handlers(const RpcRegistrar& registrar, LogBridge& log)
    {
        registrar.add(
            Wire::EDITOR_LOG,
            [&log](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor: write its line into the engine's unified log. No
                // response.
                log.write_editor_log(params);
            });
        registrar.add(
            Wire::ENGINE_SET_LOG_COLORS,
            [&log](const tbx::Json& params, tbx::RpcResponder& r)
            {
                log.set_log_colors(params);
                r.result(tbx::Json::object());
            });
    }
}
