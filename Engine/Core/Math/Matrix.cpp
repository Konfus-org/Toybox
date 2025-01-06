#include "TbxPCH.h"
#include "Matrix.h"

namespace Tbx
{
    Matrix Matrix::Identity()
    {
        return Matrix(
            { 
                1.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 1.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f 
            });
    }

    Matrix Matrix::FromPosition(const Vector3& position)
    {
        auto result = Matrix::Identity();
        result[3]  = position.X;
        result[7]  = position.Y;
        result[11] = position.Z;
        return result;
    }

    Matrix Matrix::FromRotation(const Quaternion& rotation)
    {
        auto result = Matrix::Identity();

        const auto& x = rotation.X;
        const auto& y = rotation.Y;
        const auto& z = rotation.Z;
        const auto& w = rotation.W;

        result[0]  = 1.0f - 2.0f * (y * y + z * z);
        result[1]  = 2.0f * (x * y + z * w);
        result[2]  = 2.0f * (x * z - y * w);
        result[4]  = 2.0f * (x * y - z * w);
        result[5]  = 1.0f - 2.0f * (x * x + z * z);
        result[6]  = 2.0f * (y * z + x * w);
        result[8]  = 2.0f * (x * z + y * w);
        result[9]  = 2.0f * (y * z - x * w);
        result[10] = 1.0f - 2.0f * (x * x + y * y);

        return result;
    }

    Matrix Matrix::FromScale(const Vector3& scale)
    {
        auto result = Matrix::Identity();
        result[0]  = scale.X;
        result[5]  = scale.Y;
        result[10] = scale.Z;
        return result;
    }

    Matrix Matrix::FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        auto positionMatrix = FromPosition(position);
        auto scaleMatrix = FromScale(scale);
        auto rotationMatrix = FromRotation(rotation);
        auto result = positionMatrix * rotationMatrix * scaleMatrix;
        return result;
    }

    Matrix Matrix::OrthographicProjection(const Bounds& bounds, float zNear, float zFar)
    {
        Matrix result;

        const auto& left = bounds.Left;
        const auto& right = bounds.Right;
        const auto& top = bounds.Top;
        const auto& bottom = bounds.Bottom;

        result[0]  = 2 / (right - left);
        result[5]  = 2 / (top - bottom);
        result[10] = -2 / (zFar - zNear);

        result[12] = -(right + left) / (right - left);
        result[13] = -(top + bottom) / (top - bottom);
        result[14] = -(zFar + zNear) / (zFar - zNear);

        return result;
    }

    Matrix Matrix::PerspectiveProjection(const Bounds& bounds, float zNear, float zFar)
    {
        Matrix result;

        result[0] = 2 * zNear / (bounds.Right - bounds.Left);
        result[1] = 0;
        result[2] = 0;
        result[3] = 0;

        result[4] = 0;
        result[5] = 2 * zNear / (bounds.Top - bounds.Bottom);

        result[6] = (bounds.Right + bounds.Left) / (bounds.Right - bounds.Left);
        result[7] = (bounds.Top + bounds.Bottom) / (bounds.Top - bounds.Bottom);
        result[8] = -(zFar + zNear) / (zFar - zNear);
        result[9] = -1;

        result[10] = 0;
        result[11] = 0;
        result[12] = -2 * zFar * zNear / (zFar - zNear);
        result[13] = 0;

        return result;
    }

    float Matrix::Determinant(const Matrix& matrix)
    {
        const auto& data = matrix.Values;
        float determinant = data[0] * (data[5] * (data[10] * data[15] - data[11] * data[14]) -
            data[6] * (data[9] * data[15] - data[11] * data[13]) +
            data[7] * (data[9] * data[14] - data[10] * data[13])) -
            data[1] * (data[4] * (data[10] * data[15] - data[11] * data[14]) -
                data[6] * (data[8] * data[15] - data[11] * data[12]) +
                data[7] * (data[8] * data[14] - data[10] * data[12])) +
            data[2] * (data[4] * (data[9] * data[15] - data[11] * data[13]) -
                data[5] * (data[8] * data[15] - data[11] * data[12]) +
                data[7] * (data[8] * data[13] - data[9] * data[12])) -
            data[3] * (data[4] * (data[9] * data[14] - data[10] * data[13]) -
                data[5] * (data[8] * data[14] - data[10] * data[12]) +
                data[6] * (data[8] * data[13] - data[9] * data[12]));
        return determinant;
    }

    Matrix Matrix::Inverse(const Matrix& matrix)
    {
        std::array<float, 16> inv;

        inv[0] = matrix[5] * matrix[10] * matrix[15] -
            matrix[5] * matrix[11] * matrix[14] -
            matrix[9] * matrix[6] * matrix[15] +
            matrix[9] * matrix[7] * matrix[14] +
            matrix[13] * matrix[6] * matrix[11] -
            matrix[13] * matrix[7] * matrix[10];

        inv[4] = -matrix[4] * matrix[10] * matrix[15] +
            matrix[4] * matrix[11] * matrix[14] +
            matrix[8] * matrix[6] * matrix[15] -
            matrix[8] * matrix[7] * matrix[14] -
            matrix[12] * matrix[6] * matrix[11] +
            matrix[12] * matrix[7] * matrix[10];

        inv[8] = matrix[4] * matrix[9] * matrix[15] -
            matrix[4] * matrix[11] * matrix[13] -
            matrix[8] * matrix[5] * matrix[15] +
            matrix[8] * matrix[7] * matrix[13] +
            matrix[12] * matrix[5] * matrix[11] -
            matrix[12] * matrix[7] * matrix[9];

        inv[12] = -matrix[4] * matrix[9] * matrix[14] +
            matrix[4] * matrix[10] * matrix[13] +
            matrix[8] * matrix[5] * matrix[14] -
            matrix[8] * matrix[6] * matrix[13] -
            matrix[12] * matrix[5] * matrix[10] +
            matrix[12] * matrix[6] * matrix[9];

        inv[1] = -matrix[1] * matrix[10] * matrix[15] +
            matrix[1] * matrix[11] * matrix[14] +
            matrix[9] * matrix[2] * matrix[15] -
            matrix[9] * matrix[3] * matrix[14] -
            matrix[13] * matrix[2] * matrix[11] +
            matrix[13] * matrix[3] * matrix[10];

        inv[5] = matrix[0] * matrix[10] * matrix[15] -
            matrix[0] * matrix[11] * matrix[14] -
            matrix[8] * matrix[2] * matrix[15] +
            matrix[8] * matrix[3] * matrix[14] +
            matrix[12] * matrix[2] * matrix[11] -
            matrix[12] * matrix[3] * matrix[10];

        inv[9] = -matrix[0] * matrix[9] * matrix[15] +
            matrix[0] * matrix[11] * matrix[13] +
            matrix[8] * matrix[1] * matrix[15] -
            matrix[8] * matrix[3] * matrix[13] -
            matrix[12] * matrix[1] * matrix[11] +
            matrix[12] * matrix[3] * matrix[9];

        inv[13] = matrix[0] * matrix[9] * matrix[14] -
            matrix[0] * matrix[10] * matrix[13] -
            matrix[8] * matrix[1] * matrix[14] +
            matrix[8] * matrix[2] * matrix[13] +
            matrix[12] * matrix[1] * matrix[10] -
            matrix[12] * matrix[2] * matrix[9];

        inv[2] = matrix[1] * matrix[6] * matrix[15] -
            matrix[1] * matrix[7] * matrix[14] -
            matrix[5] * matrix[2] * matrix[15] +
            matrix[5] * matrix[3] * matrix[14] +
            matrix[13] * matrix[2] * matrix[7] -
            matrix[13] * matrix[3] * matrix[6];

        inv[6] = -matrix[0] * matrix[6] * matrix[15] +
            matrix[0] * matrix[7] * matrix[14] +
            matrix[4] * matrix[2] * matrix[15] -
            matrix[4] * matrix[3] * matrix[14] -
            matrix[12] * matrix[2] * matrix[7] +
            matrix[12] * matrix[3] * matrix[6];

        inv[10] = matrix[0] * matrix[5] * matrix[15] -
            matrix[0] * matrix[7] * matrix[13] -
            matrix[4] * matrix[1] * matrix[15] +
            matrix[4] * matrix[3] * matrix[13] +
            matrix[12] * matrix[1] * matrix[7] -
            matrix[12] * matrix[3] * matrix[5];

        inv[14] = -matrix[0] * matrix[5] * matrix[14] +
            matrix[0] * matrix[6] * matrix[13] +
            matrix[4] * matrix[1] * matrix[14] -
            matrix[4] * matrix[2] * matrix[13] -
            matrix[12] * matrix[1] * matrix[6] +
            matrix[12] * matrix[2] * matrix[5];

        inv[3] = -matrix[1] * matrix[6] * matrix[11] +
            matrix[1] * matrix[7] * matrix[10] +
            matrix[5] * matrix[2] * matrix[11] -
            matrix[5] * matrix[3] * matrix[10] -
            matrix[9] * matrix[2] * matrix[7] +
            matrix[9] * matrix[3] * matrix[6];

        inv[7] = matrix[0] * matrix[6] * matrix[11] -
            matrix[0] * matrix[7] * matrix[10] -
            matrix[4] * matrix[2] * matrix[11] +
            matrix[4] * matrix[3] * matrix[10] +
            matrix[8] * matrix[2] * matrix[7] -
            matrix[8] * matrix[3] * matrix[6];

        inv[11] = -matrix[0] * matrix[5] * matrix[11] +
            matrix[0] * matrix[7] * matrix[9] +
            matrix[4] * matrix[1] * matrix[11] -
            matrix[4] * matrix[3] * matrix[9] -
            matrix[8] * matrix[1] * matrix[7] +
            matrix[8] * matrix[3] * matrix[5];

        inv[15] = matrix[0] * matrix[5] * matrix[10] -
            matrix[0] * matrix[6] * matrix[9] -
            matrix[4] * matrix[1] * matrix[10] +
            matrix[4] * matrix[2] * matrix[9] +
            matrix[8] * matrix[1] * matrix[6] -
            matrix[8] * matrix[2] * matrix[5];

        float det = matrix[0] * inv[0] + matrix[1] * inv[4] + matrix[2] * inv[8] + matrix[3] * inv[12];
        if (det == 0) return Matrix::Identity();

        det = 1.0f / det;
        auto result = Matrix::Identity();
        for (int i = 0; i < 16; i++)
        {
            result[i] = inv[i] * det;
        }

        return result;
    }

    Matrix Matrix::Translate(const Matrix& matrix, const Vector3& translate)
    {
        auto result = matrix;

        result[3]  *= translate.X; // move along the x-axis
        result[7]  *= translate.Y; // move along the y-axis
        result[11] *= translate.Z; // move along the z-axis

        return result;
    }

    Matrix Matrix::Rotate(const Matrix& matrix, const Quaternion& rotation)
    {
        const auto& x = rotation.X;
        const auto& y = rotation.Y;
        const auto& z = rotation.Z;
        const auto& w = rotation.W;

        auto result = matrix;
        result[0]  *= 1.0f - 2.0f * (y * y + z * z);
        result[1]  *= 2.0f * (x * y + z * w);
        result[2]  *= 2.0f * (x * z - y * w);
        result[4]  *= 2.0f * (x * y - z * w);
        result[5]  *= 1.0f - 2.0f * (x * x + z * z);
        result[6]  *= 2.0f * (y * z + x * w);
        result[8]  *= 2.0f * (x * z + y * w);
        result[9]  *= 2.0f * (y * z - x * w);
        result[10] *= 1.0f - 2.0f * (x * x + y * y);

        return result;
    }

    Matrix Matrix::Scale(const Matrix& matrix, const Vector3& scale)
    {
        auto result = matrix;

        result[0]  *= scale.X; // scale along the x-axis
        result[5]  *= scale.Y; // scale along the y-axis
        result[10] *= scale.Z; // scale along the z-axis

        return result;
    }
}