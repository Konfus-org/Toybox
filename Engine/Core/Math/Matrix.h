#pragma once
#include "TbxAPI.h"
#include "TbxPCH.h"
#include "Vectors.h"
#include "Bounds.h"
#include "Quaternion.h"
#include "Operations.h"
#include "Debug/Debugging.h"

namespace Tbx
{
    struct TBX_API Matrix
    {
    public:
        Matrix() : Values(Identity()) {};
        explicit(false) Matrix(const std::array<float, 16>& data) : Values(data) {}

        std::array<float, 16> Values;

        static Matrix Identity();
        static Matrix FromPosition(const Vector3& position);
        static Matrix FromRotation(const Quaternion& rotation);
        static Matrix FromScale(const Vector3& scale);
        static Matrix FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale);
        static Matrix OrthographicProjection(const Bounds& bounds, float zNear, float zFar);
        static Matrix PerspectiveProjection(const Bounds& bounds, float zNear, float zFar);
        static float Determinant(const Matrix& matrix);
        static Matrix Inverse(const Matrix& matrix);
        static Matrix Translate(const Matrix& matrix, const Vector3& translate);
        static Matrix Rotate(const Matrix& matrix, const Quaternion& rotation);
        static Matrix Scale(const Matrix& matrix, const Vector3& scale);

        float& operator[](int index)
        {
            return Values[index];
        }

        const float& operator[](int index) const
        {
            return Values[index];
        }

        operator std::array<float, 16>() const
        {
            return Values;
        }

        friend Matrix operator*(const Matrix& lhs, const Matrix& rhs)
        {
            Matrix result;
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    result.Values[i * 4 + j] =
                        lhs.Values[i * 4] *
                        rhs.Values[j] + lhs.Values[i * 4 + 1] *
                        rhs.Values[j + 4] + lhs.Values[i * 4 + 2] *
                        rhs.Values[j + 8] + lhs.Values[i * 4 + 3] *
                        rhs.Values[j + 12];
                }
            }
            return result;
        }
    };
}