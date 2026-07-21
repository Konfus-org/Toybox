#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct EngineServices;
    struct ViewState;

    /// @brief Registers the property-connection verbs (connection.add / connection.remove — the
    /// editor's value wires) on the shared registrar.
    void register_connection_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views);
}
