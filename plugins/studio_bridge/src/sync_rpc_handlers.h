#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    class SyncPathRouter;
    class WorldManager;

    /// @brief Registers the sync.* editor RPC methods: the uniform path-addressed property verbs
    /// (served by the SyncPathRouter) plus sync.catalog (served by the WorldManager).
    void register_sync_handlers(
        const RpcRegistrar& registrar, SyncPathRouter& sync_router, WorldManager& world_manager);
}
