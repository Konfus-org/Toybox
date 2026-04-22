#pragma once
#include "tbx/systems/math/vectors.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <functional>
#include <vector>


namespace tbx
{
    /// @brief
    /// Purpose: Selects when trigger colliders evaluate overlap queries.
    /// @details
    /// Ownership: Value enum copied by value.
    /// Thread Safety: Immutable enum values; safe for concurrent reads.
    enum class ColliderOverlapExecutionMode
    {
        AUTO = 0,
        MANUAL = 1,
    };

    /// @brief
    /// Purpose: Describes an overlap relationship produced by a trigger collider query.
    /// @details
    /// Ownership: Value type containing non-owning entity identifiers.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API ColliderOverlapEvent
    {
        Uuid trigger_entity_id = {};
        Uuid overlapped_entity_id = {};
    };

    /// @brief
    /// Purpose: Callback signature used by trigger collider overlap notifications.
    /// @details
    /// Ownership: Callback lifetime is owned by the registering trigger callback list.
    /// Thread Safety: Invoked by runtime physics update on the main thread.
    using ColliderOverlapCallback = std::function<void(const ColliderOverlapEvent&)>;

    /// @brief
    /// Purpose: Configures trigger-collider overlap behavior and callback registration.
    /// @details
    /// Ownership: Owns overlap query state and callback lists by value.
    /// Thread Safety: Not thread-safe; mutate and trigger from the main thread.
    struct TBX_API ColliderTrigger
    {
        bool is_trigger_only = false;
        bool is_overlap_enabled = false;
        bool is_manual_scan_requested = false;
        ColliderOverlapExecutionMode overlap_execution_mode = ColliderOverlapExecutionMode::AUTO;

        std::vector<ColliderOverlapCallback> overlap_begin_callbacks = {};
        std::vector<ColliderOverlapCallback> overlap_stay_callbacks = {};
        std::vector<ColliderOverlapCallback> overlap_end_callbacks = {};

        /// @brief
        /// Purpose: Requests a manual overlap query on the next physics tick.
        /// @details
        /// Ownership: Mutates this trigger collider state in place.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void request_overlap_scan();
    };

    /// @brief
    /// Purpose: Configures mesh collider behavior for geometry sourced from a mesh component on the
    /// same entity.
    /// @details
    /// Ownership: Owns mesh collider settings by value only; geometry ownership stays with the mesh
    /// component. Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API MeshCollider
    {
        bool is_convex = true;
        ColliderTrigger trigger = {};
    };

    /// @brief
    /// Purpose: Defines an axis-aligned box collider by half extents.
    /// @details
    /// Ownership: Owns size data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API CubeCollider
    {
        Vec3 half_extents = Vec3(0.5F, 0.5F, 0.5F);
        ColliderTrigger trigger = {};
    };

    /// @brief
    /// Purpose: Defines a sphere collider by radius.
    /// @details
    /// Ownership: Owns radius data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API SphereCollider
    {
        float radius = 0.5F;
        ColliderTrigger trigger = {};
    };

    /// @brief
    /// Purpose: Defines a capsule collider by radius and half-height.
    /// @details
    /// Ownership: Owns capsule dimensions by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API CapsuleCollider
    {
        float radius = 0.5F;
        float half_height = 0.5F;
        ColliderTrigger trigger = {};
    };

}
