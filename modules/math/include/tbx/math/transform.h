#pragma once
#include "tbx/debugging/printable.h"
#include "tbx/math/quaternion.h"
#include "tbx/tbx_api.h"

namespace tbx 
{
    struct TBX_API transform
    {
        transform() = default;
        transform(const vec3& position, const quat& rotation, const vec3& scale) : Position(position), Rotation(rotation), Scale(scale) {}

        Transform& SetPosition(vec3 newPos)
        {
            Position = newPos;
            return *this;
        }

        Transform& SetRotation(Quaternion newRot)
        {
            Rotation = newRot;
            return *this;
        }

        Transform& SetScale(vec3 newScale)
        {
            Scale = newScale;
            return *this;
        }

        vec3 position = vec3::zero;
        quat rotation = Quaternion::Identity;
        vec3 scale = Vector3::One;
    };
}
