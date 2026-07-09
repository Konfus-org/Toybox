#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    class RpcRegistrar;
    struct ViewState;

    /// @brief Registers the world-level (and app-settings) editor RPC methods served by the world_ops
    /// free functions.
    void register_world_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views);
}
