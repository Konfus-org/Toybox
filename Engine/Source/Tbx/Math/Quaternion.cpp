#include "Tbx/PCH.h"
#include "Tbx/Math/Quaternion.h"
#include "Tbx/Math/Trig.h"
#include <glm/fwd.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Tbx
{
    Vector3 Quaternion::GetForward(const Quaternion& rot)
    {
        const auto glmQuat = glm::quat(rot.W, rot.X, rot.Y, rot.Z);
        glm::vec3 objectForward = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 result = glm::normalize(glm::rotate(glmQuat, objectForward));
        return { result.x, result.y, result.z };
    }

    Vector3 Quaternion::GetRight(const Quaternion& rot)
    {
        const auto glmQuat = glm::quat(rot.W, rot.X, rot.Y, rot.Z);
        glm::vec3 objectRight = glm::vec3(-1.0f, 0.0f, 0.0f);
        glm::vec3 result = glm::normalize(glm::rotate(glmQuat, objectRight));
        return { result.x, result.y, result.z };
    }

    Vector3 Quaternion::GetUp(const Quaternion& rot)
    {
        const auto glmQuat = glm::quat(rot.W, rot.X, rot.Y, rot.Z);
        glm::vec3 objectUp = glm::vec3(0.0f, 1.0f, 0.0f); 
        glm::vec3 result = glm::normalize(glm::rotate(glmQuat, objectUp));
        return { result.x, result.y, result.z };
    }

    Quaternion Quaternion::FromAxisAngle(const Vector3& axis, float angle)
    {
        glm::vec3 glmAxis = glm::normalize(glm::vec3(axis.X, axis.Y, axis.Z)); // Normalize the axis
        glm::quat result = glm::angleAxis(Math::DegreesToRadians(angle), glmAxis); // Convert angle to radians
        return { result.x, result.y, result.z, result.w };
    }

    Quaternion Quaternion::FromEuler(float x, float y, float z)
    {
        const auto result = glm::quat(glm::vec3(Math::DegreesToRadians(x), Math::DegreesToRadians(y), Math::DegreesToRadians(z)));
        return {result.x, result.y, -result.z, result.w};
    }

    Vector3 Quaternion::ToEuler(const Quaternion& quaternion)
    {
        glm::vec3 result = glm::degrees(glm::eulerAngles(glm::quat(quaternion.W, quaternion.X, quaternion.Y, quaternion.Z)));
        return {result.x, result.y, result.z};
    }

    bool Quaternion::IsEqualOrEquivalent(const Quaternion& lhs, const Quaternion& rhs, float epsilon)
    {
        // If q and -q are both valid rotations, check both possibilities
        bool directMatch =
            std::abs(lhs.X - rhs.X) < epsilon &&
            std::abs(lhs.Y - rhs.Y) < epsilon &&
            std::abs(lhs.Z - rhs.Z) < epsilon &&
            std::abs(lhs.W - rhs.W) < epsilon;

        bool negatedMatch =
            std::abs(lhs.X + rhs.X) < epsilon &&
            std::abs(lhs.Y + rhs.Y) < epsilon &&
            std::abs(lhs.Z + rhs.Z) < epsilon &&
            std::abs(lhs.W + rhs.W) < epsilon;

        return directMatch || negatedMatch;
    }

    std::string Quaternion::ToString() const
    {
        return std::format("(X: {}, Y: {}, Z: {}, W: {})", X, Y, Z, W);
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
        return { result.x, result.y, result.z, result.w };
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
