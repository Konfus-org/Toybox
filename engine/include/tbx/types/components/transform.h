#pragma once
#include "tbx/types/components/component.h"
#include "tbx/types/components/transform.generated.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    // Describes a local-space position, rotation, and scale triple for composing spatial
    // transforms.
    // Ownership: value type; callers own instances and should copy when sharing across systems.
    // Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    [[serializable]];
    [[icon("Move3d", Color::BLUE)]];
    struct TBX_API Transform : Component
    {
        Transform();
        Transform(const Vec3& position);
        Transform(const Vec3& position, const Quat& rotation);
        Transform(const Vec3& position, const Quat& rotation, const Vec3& scale);

        /// @brief
        /// Purpose: Resolves this local transform in world space using the provided entity's parent
        /// hierarchy.
        /// @details
        /// Ownership: Returns an owned Transform value snapshot.
        /// Thread Safety: Not thread-safe; synchronize external concurrent access.
        Transform to_world_space(const class Entity& entity) const;

        // Local-space translation component for the transform.
        // Ownership: stored by value inside the transform.
        // Thread Safety: synchronize external access when sharing instances.
        [[prop]]
        [[category("Transform")]]
        [[description("Local-space position, in metres.")]]
        Vec3 position = Vec3(0.0f);

        // Local-space rotation component for the transform.
        // Ownership: stored by value inside the transform.
        // Thread Safety: synchronize external access when sharing instances.
        [[prop]]
        [[category("Transform")]]
        [[description("Local-space rotation, as a quaternion.")]]
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);

        // Local-space scale component for the transform.
        // Ownership: stored by value inside the transform.
        // Thread Safety: synchronize external access when sharing instances.
        [[prop]]
        [[category("Transform")]]
        [[description("Local-space scale multiplier per axis.")]]
        Vec3 scale = Vec3(1.0f);
    };

    /// @brief
    /// Purpose: Composes a parent world-space transform with a child local-space transform.
    /// @details
    /// Ownership: Returns an owned Transform value.
    /// Thread Safety: Stateless helper; safe to call concurrently.
    inline Transform compose_world_space_transform(
        const Transform& parent_transform,
        const Transform& local_transform)
    {
        auto world_transform = Transform {};
        world_transform.scale = parent_transform.scale * local_transform.scale;
        world_transform.rotation = normalize(parent_transform.rotation * local_transform.rotation);
        world_transform.position =
            parent_transform.position
            + (parent_transform.rotation * (parent_transform.scale * local_transform.position));
        return world_transform;
    }

    /// @brief
    /// Purpose: Converts a world-space transform into a local-space transform relative to a parent
    /// world-space transform.
    /// @details
    /// Ownership: Returns an owned Transform value.
    /// Thread Safety: Stateless helper; safe to call concurrently.
    TBX_API Transform
        world_to_local_tranform(const Transform& parent_world, const Transform& world);
}
