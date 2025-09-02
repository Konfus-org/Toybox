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

    Vector2& Vector2::operator+=(const Vector2& other)
    {
        X += other.X;
        Y += other.Y;
        return *this;
    }

    Vector2& Vector2::operator-=(const Vector2& other)
    {
        X -= other.X;
        Y -= other.Y;
        return *this;
    }

    Vector2& Vector2::operator*=(const Vector2& other)
    {
        X *= other.X;
        Y *= other.Y;
        return *this;
    }

    Vector2& Vector2::operator*=(float other)
    {
        X *= other;
        Y *= other;
        return *this;
    }

    std::string Vector2::ToString() const
    {
        return std::format("({}, {})", X, Y);
    }

    bool Vector2::IsNearlyZero(float tolerance) const
    {
        return glm::abs(X) < tolerance &&
            glm::abs(Y) < tolerance;
    }

    Vector2 Vector2::Normalize(const Vector2& vector)
    {
        const auto& glmVec = glm::vec2(vector.X, vector.Y);
        const auto& result = glm::normalize(glmVec);
        return { result.x, result.y };
    }

    Vector2 Vector2::Add(const Vector2& lhs, const Vector2& rhs)
    {
        const auto& glmVecL = glm::vec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::vec2(rhs.X, rhs.Y);

        const auto& result = glmVecL + glmVecR;
        return { result.x, result.y };
    }

    Vector2 Vector2::Subtract(const Vector2& lhs, const Vector2& rhs)
    {
        const auto& glmVecL = glm::vec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::vec2(rhs.X, rhs.Y);

        const auto& result = glmVecL - glmVecR;
        return { result.x, result.y };
    }

    Vector2 Vector2::Multiply(const Vector2& lhs, const Vector2& rhs)
    {
        const auto& glmVecL = glm::vec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::vec2(rhs.X, rhs.Y);

        const auto& result = glmVecL * glmVecR;
        return { result.x, result.y };
    }

    Vector2 Vector2::Multiply(const Vector2& lhs, float scalar)
    {
        const auto& glmVecL = glm::vec2(lhs.X, lhs.Y);
        const auto& result = glmVecL * scalar;
        return { result.x, result.y };
    }

    float Vector2::Dot(const Vector2& lhs, const Vector2& rhs)
    {
        const auto& glmVecL = glm::vec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::vec2(rhs.X, rhs.Y);

        const auto& result = glm::dot(glmVecL, glmVecR);
        return result;
    }

    Vector2I& Vector2I::operator+=(const Vector2I& other)
    {
        X += other.X;
        Y += other.Y;
        return *this;
    }

    Vector2I& Vector2I::operator-=(const Vector2I& other)
    {
        X -= other.X;
        Y -= other.Y;
        return *this;
    }

    Vector2I& Vector2I::operator*=(const Vector2I& other)
    {
        X *= other.X;
        Y *= other.Y;
        return *this;
    }

    Vector2I& Vector2I::operator*=(int other)
    {
        X *= other;
        Y *= other;
        return *this;
    }

    std::string Vector2I::ToString() const
    {
        return std::format("({}, {})", X, Y);
    }

    bool Vector2I::IsNearlyZero(int tolerance) const
    {
        return glm::abs(X) <= tolerance &&
            glm::abs(Y) <= tolerance;
    }

    Vector2I Vector2I::Normalize(const Vector2I& vector)
    {
        const auto& glmVec = glm::vec2(static_cast<float>(vector.X), static_cast<float>(vector.Y));
        const auto& result = glm::normalize(glmVec);
        return { static_cast<int>(result.x), static_cast<int>(result.y) };
    }

    Vector2I Vector2I::Add(const Vector2I& lhs, const Vector2I& rhs)
    {
        const auto& glmVecL = glm::ivec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::ivec2(rhs.X, rhs.Y);

        const auto& result = glmVecL + glmVecR;
        return { result.x, result.y };
    }

    Vector2I Vector2I::Subtract(const Vector2I& lhs, const Vector2I& rhs)
    {
        const auto& glmVecL = glm::ivec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::ivec2(rhs.X, rhs.Y);

        const auto& result = glmVecL - glmVecR;
        return { result.x, result.y };
    }

    Vector2I Vector2I::Multiply(const Vector2I& lhs, const Vector2I& rhs)
    {
        const auto& glmVecL = glm::ivec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::ivec2(rhs.X, rhs.Y);

        const auto& result = glmVecL * glmVecR;
        return { result.x, result.y };
    }

    Vector2I Vector2I::Multiply(const Vector2I& lhs, int scalar)
    {
        const auto& glmVecL = glm::ivec2(lhs.X, lhs.Y);
        const auto& result = glmVecL * scalar;
        return { result.x, result.y };
    }

    int Vector2I::Dot(const Vector2I& lhs, const Vector2I& rhs)
    {
        const auto& glmVecL = glm::ivec2(lhs.X, lhs.Y);
        const auto& glmVecR = glm::ivec2(rhs.X, rhs.Y);

        const auto& result = glm::dot(glmVecL, glmVecR);
        return result;
    }
}
