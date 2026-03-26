#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace jolt_physics
{
    /// <summary>Returns the object layer used for non-moving physics bodies.</summary>
    /// <remarks>
    /// Purpose: Provides a shared layer value for static colliders in the plugin.
    /// Ownership: Returns a value; no ownership transfer.
    /// Thread Safety: Thread-safe and immutable.
    /// </remarks>
    JPH::ObjectLayer get_static_object_layer();

    /// <summary>Returns the object layer used for moving physics bodies.</summary>
    /// <remarks>
    /// Purpose: Provides a shared layer value for dynamic/kinematic colliders in the plugin.
    /// Ownership: Returns a value; no ownership transfer.
    /// Thread Safety: Thread-safe and immutable.
    /// </remarks>
    JPH::ObjectLayer get_moving_object_layer();

    /// <summary>Provides the broad phase layer interface used by the plugin physics system.</summary>
    /// <remarks>
    /// Purpose: Maps object layers to broad phase layers for Jolt initialization.
    /// Ownership: Returns a non-owning reference to a process-lifetime singleton.
    /// Thread Safety: Thread-safe for concurrent read-only access.
    /// </remarks>
    const JPH::BroadPhaseLayerInterface& get_broad_phase_layer_interface();

    /// <summary>Provides the object-vs-broad-phase layer filter used by the plugin.</summary>
    /// <remarks>
    /// Purpose: Defines which object layers can collide with broad phase layers.
    /// Ownership: Returns a non-owning reference to a process-lifetime singleton.
    /// Thread Safety: Thread-safe for concurrent read-only access.
    /// </remarks>
    const JPH::ObjectVsBroadPhaseLayerFilter& get_object_vs_broad_phase_layer_filter();

    /// <summary>Provides the object layer pair filter used by the plugin.</summary>
    /// <remarks>
    /// Purpose: Defines which object layer pairs should collide.
    /// Ownership: Returns a non-owning reference to a process-lifetime singleton.
    /// Thread Safety: Thread-safe for concurrent read-only access.
    /// </remarks>
    const JPH::ObjectLayerPairFilter& get_object_layer_pair_filter();
}
