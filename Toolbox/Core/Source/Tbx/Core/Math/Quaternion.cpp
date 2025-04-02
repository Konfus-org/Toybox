#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Math/Quaternion.h"
#include "Tbx/Core/Math/Trig.h"
#include <glm/fwd.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Tbx
{
    Quaternion Quaternion::Identity()
    {
        const auto& glmQuat = glm::identity<glm::quat>();
        return Quaternion(glmQuat.x, glmQuat.y, glmQuat.z, glmQuat.w);
    }

    Quaternion Quaternion::FromEuler(float x, float y, float z)
    {
        glm::quat result = glm::vec3(Math::DegreesToRadians(x), Math::DegreesToRadians(y), Math::DegreesToRadians(z));
        return Quaternion(result.x, result.y, result.z, result.w);
    }

    Vector3 Quaternion::ToEuler(const Quaternion& quaternion)
    {
        glm::vec3 result = glm::eulerAngles(glm::quat(quaternion.W, quaternion.X, quaternion.Y, quaternion.Z));
        return Vector3(Math::RadiansToDegrees(result.x), Math::RadiansToDegrees(result.y), Math::RadiansToDegrees(result.z));
    }

    Quaternion Quaternion::Normalize(const Quaternion& quaternion)
    {
        const auto& glmQuat = glm::quat(quaternion.W, quaternion.X, quaternion.Y, quaternion.Z);
        const auto& result = glm::normalize(glmQuat);

        return Quaternion(result.x, result.y, result.z, result.w);
    }

    Quaternion Quaternion::Add(const Quaternion& lhs, const Quaternion& rhs)
    {
        const auto& glmL = glm::quat(lhs.W, lhs.X, lhs.Y, lhs.Z);
        const auto& glmR = glm::quat(rhs.W, rhs.X, rhs.Y, rhs.Z);

        auto result = glmL + glmR;
        return Quaternion(result.x, result.y, result.z, result.w);
    }

    Quaternion Quaternion::Subtract(const Quaternion& lhs, const Quaternion& rhs)
    {
        const auto& glmL = glm::quat(rhs.W, lhs.X, lhs.Y, lhs.Z);
        const auto& glmR = glm::quat(lhs.W, rhs.X, rhs.Y, rhs.Z);

        auto result = glmL - glmR;
        return Quaternion(result.x, result.y, result.z, result.w);
    }

    Quaternion Quaternion::Multiply(const Quaternion& lhs, const Quaternion& rhs)
    {
        const auto& glmL = glm::quat(lhs.W, lhs.X, lhs.Y, lhs.Z);
        const auto& glmR = glm::quat(rhs.W, rhs.X, rhs.Y, rhs.Z);

        auto result = glmL * glmR;
        return Quaternion(result.x, result.y, result.z, result.w);
    }
}
