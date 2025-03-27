#pragma once
#include "ToolboxAPI.h"
#include "Vectors.h"
#include "Bounds.h"
#include "Quaternion.h"

namespace Tbx
{
    struct TBX_API Matrix
    {
    public:
        Matrix() : Values(Identity()) {};
        explicit(false) Matrix(const std::array<float, 16>& data) : Values(data) {}

        std::array<float, 16> Values;

        std::string ToString() const;

        static Matrix Zero();
        static Matrix Identity();

        static Matrix FromPosition(const Vector3& position);
        static Matrix FromRotation(const Quaternion& rotation);
        static Matrix FromScale(const Vector3& scale);
        static Matrix FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale);

        static Matrix LookAt(Vector3 from, Vector3 target, Vector3 up);
        static Matrix OrthographicProjection(const Bounds& bounds, float zNear, float zFar);
        static Matrix PerspectiveProjection(float fov, float aspect, float zNear, float zFar);

        static Matrix Inverse(const Matrix& matrix);

        static Matrix Translate(const Matrix& matrix, const Vector3& translate);
        static Matrix Rotate(const Matrix& matrix, const Quaternion& rotation);
        static Matrix Scale(const Matrix& matrix, const Vector3& scale);

        static Matrix Add(const Matrix& lhs, const Matrix& rhs);
        static Matrix Subtract(const Matrix& lhs, const Matrix& rhs);
        static Matrix Multiply(const Matrix& lhs, const Matrix& rhs);
        static Matrix Multiply(float lhs, const Matrix& rhs);
        static Matrix Multiply(const Matrix& lhs, float rhs);

        float& operator[](int index) { return Values[index]; }
        const float& operator[](int index) const { return Values[index]; }
        explicit(false) operator std::array<float, 16>() const { return Values; }

        friend Matrix operator*(float lhs, const Matrix& rhs) { return Multiply(lhs, rhs); }
        friend Matrix operator*(const Matrix& lhs, float rhs) { return Multiply(lhs, rhs); }
        friend Matrix operator*(const Matrix& lhs, const Matrix& rhs) { return Multiply(lhs, rhs); }
        friend Matrix operator+(const Matrix& lhs, const Matrix& rhs) { return Add(lhs, rhs); }
        friend Matrix operator-(const Matrix& lhs, const Matrix& rhs) { return Subtract(lhs, rhs); }
    };
}