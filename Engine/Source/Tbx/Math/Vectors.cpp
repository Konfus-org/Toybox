#include "Tbx/PCH.h"
#include "Tbx/Math/Vectors.h"
#include <glm/glm.hpp>

namespace Tbx
{
    Vector3::Vector3(const Vector2& vector)
    {
        X = vector.X;
        Y = vector.Y;
        Z = 0.0f;
    }

    Vector3::Vector3(const Vector2I& vector)
    {
        X = static_cast<float>(vector.X);
        Y = static_cast<float>(vector.Y);
        Z = 0.0f;
    }

    Vector3& Vector3::operator+=(const Vector3& other)
    {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }

    Vector3& Vector3::operator-=(const Vector3& other)
    {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        return *this;
    }

    Vector3& Vector3::operator*=(const Vector3& other)
    {
        X *= other.X;
        Y *= other.Y;
        Z *= other.Z;
        return *this;
    }

    Vector3& Vector3::operator*=(float other)
    {
        X *= other;
        Y *= other;
        Z *= other;
        return *this;
    }

    std::string Vector3::ToString() const
    {
        return std::format("({}, {}, {})", X, Y, Z);
    }

    bool Vector3::IsNearlyZero(float tolerance) const
    {
        return glm::abs(X) < tolerance &&
            glm::abs(Y) < tolerance &&
            glm::abs(Z) < tolerance;
    }

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

    std::string Vector2::ToString() const
    {
        return std::format("({}, {})", X, Y);
    }

    std::string Vector2I::ToString() const
    {
        return std::format("({}, {})", X, Y);
    }
}
