#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/component.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    // Describes a local-space position, rotation, and scale triple for composing spatial
    // transforms.
    // Ownership: value type; callers own instances and should copy when sharing across systems.
    // Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    [[tbx::serializable]];
    [[tbx::prop(id, position, rotation, scale)]];
    struct TBX_API Transform : Component
    {
        Transform();
        Transform(const Vec3& position);
        Transform(const Vec3& position, const Quat& rotation);
        Transform(const Vec3& position, const Quat& rotation, const Vec3& scale);

        // Local-space translation component for the transform.
        // Ownership: stored by value inside the transform.
        // Thread Safety: synchronize external access when sharing instances.
        Vec3 position = Vec3(0.0f);

        // Local-space rotation component for the transform.
        // Ownership: stored by value inside the transform.
        // Thread Safety: synchronize external access when sharing instances.
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);

        // Local-space scale component for the transform.
        // Ownership: stored by value inside the transform.
        // Thread Safety: synchronize external access when sharing instances.
        Vec3 scale = Vec3(1.0f);
    };

    /// @brief
    /// Purpose: Converts a world-space transform into a local-space transform relative to a parent
    /// world-space transform.
    /// @details
    /// Ownership: Returns an owned Transform value.
    /// Thread Safety: Stateless helper; safe to call concurrently.
    TBX_API Transform
        world_to_local_tranform(const Transform& parent_world, const Transform& world);
}

#include "tbx/types/components/transform.generated.h"
