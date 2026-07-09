#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    class RpcRegistrar;
    struct ViewState;

    /// @brief Registers the asset.* (and asset-facing editor.*) RPC methods. The catalog ops are served
    /// by the asset_ops free functions; the preview-material query lives in world_ops.
    void register_asset_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views);
}
