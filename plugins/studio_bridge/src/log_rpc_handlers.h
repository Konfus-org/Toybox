#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    class LogBridge;

    /// @brief Registers the logging editor RPC methods served by the LogBridge.
    void register_log_handlers(const RpcRegistrar& registrar, LogBridge& log);
}
