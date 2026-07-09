#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct SyncEventState;

    /// @brief Attaches the bridge's forwarding callbacks to the trigger/collider components of every
    /// entity named by a physics-event subscription (the editor's sync.subscribe), so their overlaps
    /// and contacts stream to the editor as sync.event raises — the wire side of the editor's
    /// Trigger/Collider component events. Looks each entity up in the active world; idempotent per
    /// entity (tracked in events.bound_entities). Called when play starts — the component callback
    /// lists are runtime-only, so each play session rebuilds them onto the freshly (re)built
    /// entities — and again when a subscription arrives mid-play.
    void bind_subscribed_physics_events(SyncEventState& events, const EngineServices& services);

    /// @brief Clears the forwarding callbacks off every bound entity's components (looking each up in
    /// the active world) and forgets them. Used on plugin detach so no live component keeps a callback
    /// into freed bridge state. Leaving play already drops them by rebuilding the entities.
    void unbind_all_physics_events(SyncEventState& events, const EngineServices& services);
}
