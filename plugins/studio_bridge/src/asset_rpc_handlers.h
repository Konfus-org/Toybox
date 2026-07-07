#pragma once

namespace tbx::studio_bridge
{
    class AssetOps;
    class RpcRegistrar;
    class WorldManager;

    /// @brief Registers the asset.* (and asset-facing editor.*) RPC methods. The catalog ops are served
    /// by AssetOps; the model-slot and preview-material queries stay on the WorldManager.
    void register_asset_handlers(
        const RpcRegistrar& registrar, AssetOps& asset_ops, WorldManager& world_manager);
}
