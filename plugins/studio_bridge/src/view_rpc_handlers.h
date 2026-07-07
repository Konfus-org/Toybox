#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    class ViewManager;

    /// @brief Registers the render-view editor RPC methods served by the ViewManager.
    void register_view_handlers(const RpcRegistrar& registrar, ViewManager& views);
}
