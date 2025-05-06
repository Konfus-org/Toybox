#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Vectors.h"
#include "Tbx/Core/Math/Quaternion.h"

namespace Tbx
{
    /// <summary>
    /// A transform is a combination of position, rotation, and scale.
    /// It is used to represent the position, rotation, and scale of an object in 3D space.
    /// </summary>
    struct EXPORT Transform
    {
        Transform() = default;
        Transform(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
            : Position(position), Rotation(rotation), Scale(scale) {}

        Vector3 Position = Vector3::Zero();
        Quaternion Rotation = Quaternion::Identity();
        Vector3 Scale = Vector3::One();
    };
}

