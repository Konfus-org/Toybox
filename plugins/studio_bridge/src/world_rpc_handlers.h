#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    class WorldManager;

    /// @brief Registers the world-level (and app-settings) editor RPC methods served by the WorldManager.
    void register_world_handlers(const RpcRegistrar& registrar, WorldManager& world_manager);
}
