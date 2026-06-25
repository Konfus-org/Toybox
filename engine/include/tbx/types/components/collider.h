#pragma once
#include "tbx/types/components/collider.generated.h"
#include "tbx/types/components/component.h"
#include "tbx/types/vectors.h"
#include <functional>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Selects when a trigger evaluates overlap queries.
    /// @details
    /// Ownership: Value enum copied by value. Thread Safety: Immutable; safe for concurrent reads.
    [[serializable]];
    enum class ColliderOverlapExecutionMode
    {
        AUTO [[name("auto")]] = 0,
        MANUAL [[name("manual")]] = 1,
    };

    /// @brief
    /// Purpose: Describes an overlap relationship produced by a trigger query.
    /// @details
    /// Ownership: Value type containing non-owning entity identifiers.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API ColliderOverlapEvent
    {
        Uuid trigger_entity_id = {};
        Uuid overlapped_entity_id = {};
    };

    /// @brief
    /// Purpose: Callback signature used by trigger overlap notifications.
    /// @details
    /// Ownership: Callback lifetime is owned by the registering trigger's callback list.
    /// Thread Safety: Invoked by the runtime physics update on the main thread.
    using ColliderOverlapCallback = std::function<void(const ColliderOverlapEvent&)>;

    /// @brief
    /// Purpose: Shared overlap behavior for the shape-specific trigger components (BoxTrigger,
    /// SphereTrigger, CapsuleTrigger, MeshTrigger). Not added to an entity directly — use a shaped
    /// trigger.
    /// @details
    /// A trigger is the collider's overlap behavior split into its own component: it reports
    /// begin/stay/end overlaps with other physics bodies through its callback lists. An entity with a
    /// trigger but no collider of the same shape is a sensor (no physical collision). All overlap
    /// callbacks and the pending-scan request are runtime-only and never serialized.
    /// Ownership: Owns overlap settings by value; callback lifetimes are owned by the lists.
    /// Thread Safety: Not thread-safe; mutate and trigger from the main thread.
    [[serializable]];
    struct TBX_API Trigger : Component
    {
        ColliderOverlapExecutionMode overlap_execution_mode = ColliderOverlapExecutionMode::AUTO;

        bool is_overlap_enabled = true;

        // Runtime overlap state — set at play time and never persisted.
        [[do_not_serialize]]
        bool is_manual_scan_requested = false;
        [[do_not_serialize]]
        std::vector<ColliderOverlapCallback> overlap_begin_callbacks = {};
        [[do_not_serialize]]
        std::vector<ColliderOverlapCallback> overlap_stay_callbacks = {};
        [[do_not_serialize]]
        std::vector<ColliderOverlapCallback> overlap_end_callbacks = {};

        /// @brief
        /// Purpose: Requests a manual overlap query on the next physics tick.
        /// @details
        /// Ownership: Mutates this trigger state in place.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void request_overlap_scan();
    };

    /// @brief Purpose: A solid axis-aligned box collision shape defined by half extents.
    [[serializable]];
    [[icon("Box", Color::GREEN)]];
    struct TBX_API BoxCollider : Component
    {
        BoxCollider() = default;
        explicit BoxCollider(Vec3 collider_half_extents);

        Vec3 half_extents = Vec3(0.5F, 0.5F, 0.5F);
    };

    /// @brief Purpose: A solid sphere collision shape defined by radius.
    [[serializable]];
    [[icon("Circle", Color::GREEN)]];
    struct TBX_API SphereCollider : Component
    {
        SphereCollider() = default;
        explicit SphereCollider(float collider_radius);

        float radius = 0.5F;
    };

    /// @brief Purpose: A solid capsule collision shape defined by radius and cylinder half height.
    [[serializable]];
    [[icon("Pill", Color::GREEN)]];
    struct TBX_API CapsuleCollider : Component
    {
        CapsuleCollider() = default;
        CapsuleCollider(float collider_radius, float collider_half_height);

        float radius = 0.5F;

        float half_height = 0.5F;
    };

    /// @brief Purpose: A solid mesh collision shape sourced from the entity's model (its Renderer).
    [[serializable]];
    [[icon("Shapes", Color::GREEN)]];
    struct TBX_API MeshCollider : Component
    {
        MeshCollider() = default;
        explicit MeshCollider(bool collider_is_convex);

        bool is_convex = true;
    };

    /// @brief Purpose: A box-shaped overlap trigger defined by half extents.
    [[serializable]];
    [[icon("Box", Color::CYAN)]];
    struct TBX_API BoxTrigger : Trigger
    {
        BoxTrigger() = default;
        explicit BoxTrigger(Vec3 trigger_half_extents);

        Vec3 half_extents = Vec3(0.5F, 0.5F, 0.5F);
    };

    /// @brief Purpose: A sphere-shaped overlap trigger defined by radius.
    [[serializable]];
    [[icon("Circle", Color::CYAN)]];
    struct TBX_API SphereTrigger : Trigger
    {
        SphereTrigger() = default;
        explicit SphereTrigger(float trigger_radius);

        float radius = 0.5F;
    };

    /// @brief Purpose: A capsule-shaped overlap trigger defined by radius and cylinder half height.
    [[serializable]];
    [[icon("Pill", Color::CYAN)]];
    struct TBX_API CapsuleTrigger : Trigger
    {
        CapsuleTrigger() = default;
        CapsuleTrigger(float trigger_radius, float trigger_half_height);

        float radius = 0.5F;

        float half_height = 0.5F;
    };

    /// @brief Purpose: A mesh-shaped overlap trigger sourced from the entity's model (its Renderer).
    [[serializable]];
    [[icon("Shapes", Color::CYAN)]];
    struct TBX_API MeshTrigger : Trigger
    {
        MeshTrigger() = default;
        explicit MeshTrigger(bool trigger_is_convex);

        bool is_convex = true;
    };
}
