#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Vectors.h"
#include "Tbx/Core/Math/Bounds.h"
#include "Quaternion.h"
#include <array>
#include <string>

namespace Tbx
{
    struct EXPORT Mat4x4 : public ILoggable
    {
    public:
        Mat4x4() : Values(Identity()) {};
        explicit(false) Mat4x4(const std::array<float, 16>& data) : Values(data) {}

        float& operator[](int index) { return Values[index]; }
        const float& operator[](int index) const { return Values[index]; }
        explicit(false) operator std::array<float, 16>() const { return Values; }

        friend Mat4x4 operator*(float lhs, const Mat4x4& rhs) { return Multiply(lhs, rhs); }
        friend Mat4x4 operator*(const Mat4x4& lhs, float rhs) { return Multiply(lhs, rhs); }
        friend Mat4x4 operator*(const Mat4x4& lhs, const Mat4x4& rhs) { return Multiply(lhs, rhs); }
        friend Mat4x4 operator+(const Mat4x4& lhs, const Mat4x4& rhs) { return Add(lhs, rhs); }
        friend Mat4x4 operator-(const Mat4x4& lhs, const Mat4x4& rhs) { return Subtract(lhs, rhs); }

        std::string ToString() const override;

        static Mat4x4 Zero();
        static Mat4x4 Identity();

        static Mat4x4 FromPosition(const Vector3& position);
        static Mat4x4 FromRotation(const Quaternion& rotation);
        static Mat4x4 FromScale(const Vector3& scale);
        static Mat4x4 FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale);

        static Mat4x4 LookAt(Vector3 from, Vector3 target, Vector3 up);
        static Mat4x4 OrthographicProjection(const Bounds& bounds, float zNear, float zFar);
        static Mat4x4 PerspectiveProjection(float fov, float aspect, float zNear, float zFar);

        static Mat4x4 Inverse(const Mat4x4& matrix);

        static Mat4x4 Translate(const Mat4x4& matrix, const Vector3& translate);
        static Mat4x4 Rotate(const Mat4x4& matrix, const Quaternion& rotation);
        static Mat4x4 Scale(const Mat4x4& matrix, const Vector3& scale);

        static Mat4x4 Add(const Mat4x4& lhs, const Mat4x4& rhs);
        static Mat4x4 Subtract(const Mat4x4& lhs, const Mat4x4& rhs);
        static Mat4x4 Multiply(const Mat4x4& lhs, const Mat4x4& rhs);
        static Mat4x4 Multiply(float lhs, const Mat4x4& rhs);
        static Mat4x4 Multiply(const Mat4x4& lhs, float rhs);

        std::array<float, 16> Values;
    };
}