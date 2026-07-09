#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;

    /// @brief Registers the log editor RPC methods (editor-originated lines + log colours), served
    /// by the stateless log_ops functions.
    void register_log_handlers(const RpcRegistrar& registrar);
}
