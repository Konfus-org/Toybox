#pragma once
#include "tbx/common/uuid.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <functional>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Selects when trigger colliders evaluate overlap queries.
    /// </summary>
    /// <remarks>
    /// Ownership: Value enum copied by value.
    /// Thread Safety: Immutable enum values; safe for concurrent reads.
    /// </remarks>
    enum class ColliderOverlapExecutionMode
    {
        AUTO = 0,
        MANUAL = 1,
    };

    /// <summary>
    /// Purpose: Describes an overlap relationship produced by a trigger collider query.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type containing non-owning entity identifiers.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API ColliderOverlapEvent
    {
        Uuid trigger_entity_id = {};
        Uuid overlapped_entity_id = {};
    };

    /// <summary>
    /// Purpose: Callback signature used by trigger collider overlap notifications.
    /// </summary>
    /// <remarks>
    /// Ownership: Callback lifetime is owned by the registering trigger callback list.
    /// Thread Safety: Invoked by runtime physics update on the main thread.
    /// </remarks>
    using ColliderOverlapCallback = std::function<void(const ColliderOverlapEvent&)>;

    /// <summary>
    /// Purpose: Configures trigger-collider overlap behavior and callback registration.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns overlap query state and callback lists by value.
    /// Thread Safety: Not thread-safe; mutate and trigger from the main thread.
    /// </remarks>
    struct TBX_API ColliderTrigger
    {
        bool is_trigger_only = false;
        bool is_overlap_enabled = false;
        ColliderOverlapExecutionMode overlap_execution_mode = ColliderOverlapExecutionMode::AUTO;
        bool is_manual_scan_requested = false;

        std::vector<ColliderOverlapCallback> overlap_begin_callbacks = {};
        std::vector<ColliderOverlapCallback> overlap_stay_callbacks = {};
        std::vector<ColliderOverlapCallback> overlap_end_callbacks = {};

        /// <summary>
        /// Purpose: Requests a manual overlap query on the next physics tick.
        /// </summary>
        /// <remarks>
        /// Ownership: Mutates this trigger collider state in place.
        /// Thread Safety: Not thread-safe; call from the main thread.
        /// </remarks>
        void request_overlap_scan();
    };

    /// <summary>
    /// Purpose: Configures mesh collider behavior for geometry sourced from a mesh component on the
    /// same entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns mesh collider settings by value only; geometry ownership stays with the mesh
    /// component.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API MeshCollider
    {
        bool is_convex = true;
        ColliderTrigger trigger = {};
    };

    /// <summary>
    /// Purpose: Defines an axis-aligned box collider by half extents.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns size data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API CubeCollider
    {
        Vec3 half_extents = Vec3(0.5F, 0.5F, 0.5F);
        ColliderTrigger trigger = {};
    };

    /// <summary>
    /// Purpose: Defines a sphere collider by radius.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns radius data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API SphereCollider
    {
        float radius = 0.5F;
        ColliderTrigger trigger = {};
    };

    /// <summary>
    /// Purpose: Defines a capsule collider by radius and half-height.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns capsule dimensions by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API CapsuleCollider
    {
        float radius = 0.5F;
        float half_height = 0.5F;
        ColliderTrigger trigger = {};
    };

}
