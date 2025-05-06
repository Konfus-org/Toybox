#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Math/Quaternion.h"
#include "Tbx/Core/Math/Trig.h"
#include <glm/fwd.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

namespace Tbx
{
    Quaternion Quaternion::Identity()
    {
        const auto glmQuat = glm::identity<glm::quat>();
        return {glmQuat.x, glmQuat.y, glmQuat.z, glmQuat.w};
    }

    Quaternion Quaternion::FromAxisAngle(const Vector3& axis, float angle)
    {
        auto glmAxis = glm::vec3(Math::DegreesToRadians(axis.X), Math::DegreesToRadians(axis.Y), Math::DegreesToRadians(axis.Z));
        glm::quat result = glm::angleAxis(Math::DegreesToRadians(angle), glm::normalize(glmAxis));
        return { result.x, result.y, result.z, result.w };
    }

    Quaternion Quaternion::FromEuler(float x, float y, float z)
    {
        const auto result = glm::quat(glm::vec3(Math::DegreesToRadians(x), Math::DegreesToRadians(y), Math::DegreesToRadians(z)));
        return {result.x, result.y, result.z, result.w};
    }

    Vector3 Quaternion::ToEuler(const Quaternion& quaternion)
    {
        glm::vec3 result = glm::degrees(glm::eulerAngles(glm::quat(quaternion.W, quaternion.X, quaternion.Y, quaternion.Z)));
        return {result.x, result.y, result.z};
    }

    Quaternion Quaternion::Normalize(const Quaternion& quaternion)
    {
        const auto glmQuat = glm::quat(quaternion.W, quaternion.X, quaternion.Y, quaternion.Z);
        const auto result = glm::normalize(glmQuat);

        return {result.x, result.y, result.z, result.w};
    }

    Quaternion Quaternion::Add(const Quaternion& lhs, const Quaternion& rhs)
    {
        const auto glmL = glm::quat(lhs.W, lhs.X, lhs.Y, lhs.Z);
        const auto glmR = glm::quat(rhs.W, rhs.X, rhs.Y, rhs.Z);

        auto result = glmL + glmR;
        return {result.x, result.y, result.z, result.w};
    }

    Quaternion Quaternion::Subtract(const Quaternion& lhs, const Quaternion& rhs)
    {
        const auto glmL = glm::quat(lhs.W, lhs.X, lhs.Y, lhs.Z);
        const auto glmR = glm::quat(rhs.W, rhs.X, rhs.Y, rhs.Z);

        auto result = glmL - glmR;
        return {result.x, result.y, result.z, result.w};
    }

    Quaternion Quaternion::Multiply(const Quaternion& lhs, const Quaternion& rhs)
    {
        const auto glmL = glm::quat(lhs.W, lhs.X, lhs.Y, lhs.Z);
        const auto glmR = glm::quat(rhs.W, rhs.X, rhs.Y, rhs.Z);

        auto result = glmL * glmR;
        return {result.x, result.y, result.z, result.w};
    }

    Vector3 Quaternion::Multiply(const Quaternion& lhs, const Vector3& rhs)
    {
        const auto glmL = glm::quat(lhs.W, lhs.X, lhs.Y, lhs.Z);
        const auto glmR = glm::vec3(rhs.X, rhs.Y, rhs.Z);
        auto result = glmL * glmR;
        return { result.x, result.y, result.z };
    }

    Vector3 Quaternion::Multiply(const Vector3& lhs, const Quaternion& rhs)
    {
        const auto& glmL = glm::vec3(lhs.X, lhs.Y, lhs.Z);
        const auto& glmR = glm::quat(rhs.W, rhs.X, rhs.Y, rhs.Z);
        auto result = glmL * glmR;
        return { result.x, result.y, result.z };
    }
}
