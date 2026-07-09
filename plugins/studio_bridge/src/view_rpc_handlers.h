#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    class RpcRegistrar;
    struct ViewState;

    /// @brief Registers the render-view editor RPC methods served by the view_ops free functions.
    void register_view_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views);
}
