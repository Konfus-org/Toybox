#pragma once
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Describes a local-space position, rotation, and scale triple for composing spatial
    // transforms.
    // Ownership: value type; callers own instances and should copy when sharing across systems.
    // Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    struct TBX_API Transform
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

    /// <summary>
    /// Purpose: Converts a world-space transform into a local-space transform relative to a parent
    /// world-space transform.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an owned Transform value.
    /// Thread Safety: Stateless helper; safe to call concurrently.
    /// </remarks>
    TBX_API Transform
        world_to_local_tranform(const Transform& parent_world, const Transform& world);
}
