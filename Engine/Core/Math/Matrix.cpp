#include "TbxPCH.h"
#include "Matrix.h"

namespace Tbx
{
    Matrix Matrix::Identity()
    {
        return Matrix({ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
    }

    Matrix Matrix::FromPosition(const Vector3& position)
    {
        auto result = Matrix::Identity();
        result = Translate(result, position);
        return result;
    }

    Matrix Matrix::FromRotation(const Quaternion& rotation)
    {
        auto result = Matrix::Identity();
        result = Rotate(result, rotation);
        return result;
    }

    Matrix Matrix::FromScale(const Vector3& scale)
    {
        auto result = Matrix::Identity();
        result = Scale(result, scale);
        return result;
    }

    Matrix Matrix::FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        auto result = Matrix::Identity();
        result = Translate(result, position);
        result = Rotate(result, rotation);
        result = Scale(result, scale);
        return result;
    }

    Matrix Matrix::OrthographicProjection(const Bounds& bounds, float zNear, float zFar)
    {
        Matrix result;

        const auto& left = bounds.Left;
        const auto& right = bounds.Right;
        const auto& top = bounds.Top;
        const auto& bottom = bounds.Bottom;

        result[0] = 2.0f / (right - left);
        result[5] = 2.0f / (top - bottom);
        result[10] = 2.0f / (zFar - zNear);
        result[12] = -(right + left) / (right - left);
        result[13] = -(top + bottom) / (top - bottom);
        result[14] = -(zFar + zNear) / (zFar - zNear);

        return result;
    }

    Matrix Matrix::PerspectiveProjection(float fov, float aspect, float zNear, float zFar)
    {
        Matrix result;

        result.Values[0] = 1.0f / (aspect * tanf(fov / 2.0f));
        result.Values[5] = 1.0f / tanf(fov / 2.0f);
        result.Values[10] = (zFar + zNear) / (zNear - zFar);
        result.Values[11] = -1.0f;
        result.Values[14] = (2.0f * zFar * zNear) / (zNear - zFar);

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
        int n = 4;
        float determinant = 0.0f;
        std::array<float, 16> data = matrix;

        // Calculate the determinant
        determinant = Determinant(matrix);

        // Matrix is not invertible
        if (determinant == 0.0f)
        {
            return Matrix::Identity();
        }

        // Create an identity matrix
        std::array<float, 16> identity = Matrix::Identity().Values;

        // Perform Gauss-Jordan elimination
        for (int i = 0; i < n; i++)
        {
            // Search for maximum in this column
            float maxEl = std::abs(data[i * n + i]);
            int maxRow = i;
            for (int k = i + 1; k < n; k++)
            {
                if (std::abs(data[k * n + i]) > maxEl)
                {
                    maxEl = std::abs(data[k * n + i]);
                    maxRow = k;
                }
            }

            // Swap maximum row with current row
            std::swap(data[i * n], data[maxRow * n]);
            std::swap(identity[i * n], identity[maxRow * n]);

            // Make all rows below this one 0 in current column
            for (int k = i + 1; k < n; k++)
            {
                float c = -data[k * n + i] / data[i * n + i];
                for (int j = 0; j < n; j++)
                {
                    if (i == j)
                    {
                        data[k * n + j] = 0.0f;
                    }
                    else
                    {
                        data[k * n + j] += c * data[i * n + j];
                    }
                    identity[k * n + j] += c * identity[i * n + j];
                }
            }
        }

        // Solve equation Ax=b for an upper triangular matrix A
        for (int i = n - 1; i >= 0; i--)
        {
            for (int k = 0; k < i; k++)
            {
                float c = -data[k * n + i] / data[i * n + i];
                for (int j = 0; j < n; j++)
                {
                    identity[k * n + j] += c * identity[i * n + j];
                }
            }
            identity[i * n + i] /= data[i * n + i];
        }

        // Return inverted matrix
        return Matrix(identity);
    }

    Matrix Matrix::Translate(const Matrix& matrix, const Vector3& translate)
    {
        auto result = matrix;

        result[0] *= translate.X; // scale along the x-axis
        result[5] *= translate.Y; // scale along the y-axis
        result[10] *= translate.Z; // scale along the z-axis

        return result;
    }

    Matrix Matrix::Rotate(const Matrix& matrix, const Quaternion& rotation)
    {
        const auto& x = rotation.X;
        const auto& y = rotation.Y;
        const auto& z = rotation.Z;
        const auto& w = rotation.W;

        auto result = matrix;
        result[0] = 1.0f - 2.0f * (y * y + z * z);
        result[1] = 2.0f * (x * y + z * w);
        result[2] = 2.0f * (x * z - y * w);
        result[4] = 2.0f * (x * y - z * w);
        result[5] = 1.0f - 2.0f * (x * x + z * z);
        result[6] = 2.0f * (y * z + x * w);
        result[8] = 2.0f * (x * z + y * w);
        result[9] = 2.0f * (y * z - x * w);
        result[10] = 1.0f - 2.0f * (x * x + y * y);

        return result;
    }

    Matrix Matrix::Scale(const Matrix& matrix, const Vector3& scale)
    {
        auto result = matrix;

        result[0] *= scale.X; // scale along the x-axis
        result[5] *= scale.Y; // scale along the y-axis
        result[10] *= scale.Z; // scale along the z-axis

        return result;
    }
}