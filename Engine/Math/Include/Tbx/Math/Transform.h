#pragma once
#include "Tbx/Math/DllExport.h"
#include "Tbx/Math/Constants.h"
#include "Tbx/Math/Quaternion.h"

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

        std::string ToString() const;

        Vector3 Position = Constants::Vector3::Zero;
        Quaternion Rotation = Constants::Quaternion::Identity;
        Vector3 Scale = Constants::Vector3::One;
    };
}

