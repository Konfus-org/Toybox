#include "render_layers_rpc_handlers.h"
#include "engine_services.h"
#include "render_layers_ops.h"
#include "render_layers_state.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_render_layers_handlers(
        const RpcRegistrar& registrar, RenderLayersState& layers, const EngineServices& services)
    {
        registrar.add(
            Wire::EDITOR_SET_RENDER_LAYERS,
            [&layers, &services](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor's render-layers toolbar; no response.
                set_render_layers(layers, services, params);
            });
    }
}
