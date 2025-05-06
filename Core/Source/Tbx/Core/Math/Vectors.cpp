#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Math/Vectors.h"
#include <glm/glm.hpp>


namespace Tbx
{
    Vector3 Vector3::Normalize(const Vector3& vector)
    {
        const auto& glmVec = glm::vec3(vector.X, vector.Y, vector.Z);
        const auto& result = glm::normalize(glmVec);
        return {result.x, result.y, result.z};
    }

    Vector3 Vector3::Add(const Vector3& lhs, const Vector3& rhs)
    {
        const auto& glmVecL = glm::vec3(lhs.X, lhs.Y, lhs.Z);
        const auto& glmVecR = glm::vec3(rhs.X, rhs.Y, rhs.Z);

        const auto& result = glmVecL + glmVecR;
        return {result.x, result.y, result.z};
    }

    Vector3 Vector3::Subtract(const Vector3& lhs, const Vector3& rhs)
    {
        const auto& glmVecL = glm::vec3(lhs.X, lhs.Y, lhs.Z);
        const auto& glmVecR = glm::vec3(rhs.X, rhs.Y, rhs.Z);

        const auto& result = glmVecL - glmVecR;
        return {result.x, result.y, result.z};
    }

    Vector3 Vector3::Multiply(const Vector3& lhs, const Vector3& rhs)
    {
        const auto& glmVecL = glm::vec3(lhs.X, lhs.Y, lhs.Z);
        const auto& glmVecR = glm::vec3(rhs.X, rhs.Y, rhs.Z);

        const auto& result = glmVecL * glmVecR;
        return {result.x, result.y, result.z};
    }

    Vector3 Vector3::Multiply(const Vector3& lhs, float scalar)
    {
        const auto& glmVecL = glm::vec3(lhs.X, lhs.Y, lhs.Z);
        const auto& result = glmVecL * scalar;
        return {result.x, result.y, result.z};
    }

    Vector3 Vector3::Cross(const Vector3& lhs, const Vector3& rhs)
    {
        const auto& glmVecL = glm::vec3(lhs.X, lhs.Y, lhs.Z);
        const auto& glmVecR = glm::vec3(rhs.X, rhs.Y, rhs.Z);

        const auto& result = glm::cross(glmVecL, glmVecR);
        return {result.x, result.y, result.z};
    }

    float Vector3::Dot(const Vector3& lhs, const Vector3& rhs)
    {
        const auto& glmVecL = glm::vec3(lhs.X, lhs.Y, lhs.Z);
        const auto& glmVecR = glm::vec3(rhs.X, rhs.Y, rhs.Z);

        const auto& result = glm::dot(glmVecL, glmVecR);
        return result;
    }
}
