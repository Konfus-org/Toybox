#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    class RpcRegistrar;
    struct ViewState;

    /// @brief Registers the entity.* editor RPC methods served by the world_ops free functions.
    void register_entity_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views);
}
