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
    struct TBX_API Matrix4x4
    {
    public:
        Matrix4x4() = default;
        explicit(false) Matrix4x4(const std::array<float, 16>& data) : Data(data) {}

        std::array<float, 16> Data;

        static Matrix4x4 Identity();
        static Matrix4x4 FromPosition(const Vector3& position);
        static Matrix4x4 FromRotation(const Quaternion& rotation);
        static Matrix4x4 FromScale(const Vector3& scale);
        static Matrix4x4 FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale);
        static Matrix4x4 OrthographicProjection(const Bounds& bounds, float zNear, float zFar);
        static Matrix4x4 PerspectiveProjection(float fov, float aspect, float zNear, float zFar);
        static float Determinant(const Matrix4x4& matrix);
        static Matrix4x4 Inverse(const Matrix4x4& matrix);
        static Matrix4x4 Translate(const Matrix4x4& matrix, const Vector3& translate);
        static Matrix4x4 Rotate(const Matrix4x4& matrix, const Quaternion& rotation);
        static Matrix4x4 Scale(const Matrix4x4& matrix, const Vector3& scale);

        float& operator[](int index)
        {
            return Data[index];
        }

        const float& operator[](int index) const
        {
            return Data[index];
        }

        operator std::array<float, 16>() const
        {
            return Data;
        }

        friend Matrix4x4 operator*(const Matrix4x4& lhs, const Matrix4x4& rhs)
        {
            Matrix4x4 result;
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    result.Data[i * 4 + j] =
                        lhs.Data[i * 4] *
                        rhs.Data[j] + lhs.Data[i * 4 + 1] *
                        rhs.Data[j + 4] + lhs.Data[i * 4 + 2] *
                        rhs.Data[j + 8] + lhs.Data[i * 4 + 3] *
                        rhs.Data[j + 12];
                }
            }
            return result;
        }
    };
}