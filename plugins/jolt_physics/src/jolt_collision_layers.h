#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace jolt_physics
{
    /// @brief
    /// Purpose: Provides a shared layer value for static colliders in the plugin.
    /// @details
    /// Ownership: Returns a value; no ownership transfer.
    /// Thread Safety: Thread-safe and immutable.

    JPH::ObjectLayer get_static_object_layer();

    /// @brief
    /// Purpose: Provides a shared layer value for dynamic/kinematic colliders in the plugin.
    /// @details
    /// Ownership: Returns a value; no ownership transfer.
    /// Thread Safety: Thread-safe and immutable.

    JPH::ObjectLayer get_moving_object_layer();

    /// @brief
    /// Purpose: Maps object layers to broad phase layers for Jolt initialization.
    /// @details
    /// Ownership: Returns a non-owning reference to a process-lifetime singleton.
    /// Thread Safety: Thread-safe for concurrent read-only access.

    const JPH::BroadPhaseLayerInterface& get_broad_phase_layer_interface();

    /// @brief
    /// Purpose: Defines which object layers can collide with broad phase layers.
    /// @details
    /// Ownership: Returns a non-owning reference to a process-lifetime singleton.
    /// Thread Safety: Thread-safe for concurrent read-only access.

    const JPH::ObjectVsBroadPhaseLayerFilter& get_object_vs_broad_phase_layer_filter();

    /// @brief
    /// Purpose: Defines which object layer pairs should collide.
    /// @details
    /// Ownership: Returns a non-owning reference to a process-lifetime singleton.
    /// Thread Safety: Thread-safe for concurrent read-only access.

    const JPH::ObjectLayerPairFilter& get_object_layer_pair_filter();
}
