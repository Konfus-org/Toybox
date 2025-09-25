#pragma once
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Quaternion.h"
#include "Tbx/Math/Bounds.h"
#include <array>
#include <string>

namespace Tbx
{
    /// <summary>
    /// A 4x4 matrix to store data. Most often used for rendering.
    /// This matrix stores data in column major order.
    /// </summary>
    struct TBX_EXPORT Mat4x4 : public IPrintable
    {
        Mat4x4();
        explicit(false) Mat4x4(const std::array<float, 16>& data) : Values(data) {}
        explicit(false) Mat4x4(const std::initializer_list<float>& data);
        explicit(false) Mat4x4(const std::initializer_list<std::initializer_list<float>>& data);
        explicit(false) Mat4x4(const std::array<std::array<float, 4>, 4>& data);

        friend Mat4x4 operator * (float lhs, const Mat4x4& rhs) { return Multiply(lhs, rhs); }
        friend Mat4x4 operator * (const Mat4x4& lhs, float rhs) { return Multiply(lhs, rhs); }
        friend Mat4x4 operator * (const Mat4x4& lhs, const Mat4x4& rhs) { return Multiply(lhs, rhs); }
        friend Mat4x4 operator + (const Mat4x4& lhs, const Mat4x4& rhs) { return Add(lhs, rhs); }
        friend Mat4x4 operator - (const Mat4x4& lhs, const Mat4x4& rhs) { return Subtract(lhs, rhs); }
        friend bool operator == (const Mat4x4& lhs, const Mat4x4& rhs) { return lhs.Values == rhs.Values; }

        float& operator[](int index) { return Values[index]; }
        const float& operator[](int index) const { return Values[index]; }
        float& operator()(int row, int col) { return Values[row * 4 + col]; }
        const float& operator()(int row, int col) const { return Values[row * 4 + col];}
        explicit(false) operator std::array<float, 16>() const { return Values; }

        std::string ToString() const override;

        static Mat4x4 FromPosition(const Vector3& position);
        static Mat4x4 FromRotation(const Quaternion& rotation);
        static Mat4x4 FromScale(const Vector3& scale);
        static Mat4x4 FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale);

        static Mat4x4 LookAt(const Vector3& from, const Vector3& target, const Vector3& up);
        static Mat4x4 OrthographicProjection(const Bounds& bounds, float zNear, float zFar);
        static Mat4x4 PerspectiveProjection(float fov, float aspect, float zNear, float zFar);

        static Mat4x4 Inverse(const Mat4x4& matrix);

        static Mat4x4 Transpose(const Mat4x4& matrix);
        static Mat4x4 Translate(const Mat4x4& matrix, const Vector3& translate);
        static Mat4x4 Rotate(const Mat4x4& matrix, float angle, const Vector3& axis);
        static Mat4x4 Scale(const Mat4x4& matrix, const Vector3& scale);

        static Mat4x4 Add(const Mat4x4& lhs, const Mat4x4& rhs);
        static Mat4x4 Subtract(const Mat4x4& lhs, const Mat4x4& rhs);
        static Mat4x4 Multiply(const Mat4x4& lhs, const Mat4x4& rhs);
        static Mat4x4 Multiply(float lhs, const Mat4x4& rhs);
        static Mat4x4 Multiply(const Mat4x4& lhs, float rhs);

        static bool IsEqual(const Mat4x4& lhs, const Mat4x4& rhs);

        static Mat4x4 Zero;
        static Mat4x4 Identity;

        /// <summary>
        /// The matrix values, stored in a flat array in row major order.
        /// </summary>
        std::array<float, 16> Values = {};
    };
}
