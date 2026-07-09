#pragma once
#include "engine_services.h"
#include "rpc_registrar.h"

namespace tbx::studio_bridge
{
    // Registers the runtime physics methods: physics.raycast (a reply-carrying query over the engine's
    // Physics service) and physics.overlapScan (a manual trigger scan request by component address).
    // The trigger/collider event raises ride the sync.event channel (see sync_event_ops /
    // physics_event_ops), not an RPC method.
    void register_physics_handlers(const RpcRegistrar& registrar, EngineServices& services);
}
