#pragma once
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Describes a position, rotation, and scale triple for composing spatial transforms.
    /// Ownership: value type; callers own instances and should copy when sharing across systems.
    /// Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    /// </summary>
    struct TBX_API Transform
    {
      public:
        Transform() = default;

        /// <summary>
        /// Initializes the transform with explicit position, rotation, and scale values.
        /// Ownership: stores provided values by copy.
        /// Thread Safety: not thread-safe; synchronize external access if shared.
        /// </summary>
        Transform(const Vec3& position, const Quat& rotation, const Vec3& scale)
            : Position(position)
            , Rotation(rotation)
            , Scale(scale)
        {
        }

        /// <summary>
        /// World-space translation component for the transform.
        /// Ownership: stored by value inside the transform.
        /// Thread Safety: synchronize external access when sharing instances.
        /// </summary>
        Vec3 Position = Vec3(0.0f);

        /// <summary>
        /// World-space rotation component for the transform.
        /// Ownership: stored by value inside the transform.
        /// Thread Safety: synchronize external access when sharing instances.
        /// </summary>
        Quat Rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);

        /// <summary>
        /// World-space scale component for the transform.
        /// Ownership: stored by value inside the transform.
        /// Thread Safety: synchronize external access when sharing instances.
        /// </summary>
        Vec3 Scale = Vec3(1.0f);
    };
}
