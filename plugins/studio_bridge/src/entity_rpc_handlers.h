#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    class WorldManager;

    /// @brief Registers the entity.* editor RPC methods served by the WorldManager.
    void register_entity_handlers(const RpcRegistrar& registrar, WorldManager& world_manager);
}
