#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Math/Mat4x4.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Tbx
{
    static Mat4x4 GlmMat4ToMatrix(const glm::mat4& glmMat)
    {
        std::array<float, 16> result;
        for (int i = 0; i < 4; i++) 
        {
            for (int j = 0; j < 4; j++) 
            {
                result[i * 4 + j] = glmMat[i][j];
            }
        }
        return result;
    }

    std::string Mat4x4::ToString() const
    {
        return std::format(
            "[{}, {}, {}, {}],\n[{}, {}, {}, {}],\n[{}, {}, {}, {}],\n[{}, {}, {}, {}]",
            Values[0], Values[1], Values[2], Values[3],
            Values[4], Values[5], Values[6], Values[7],
            Values[8], Values[9], Values[10], Values[11],
            Values[12], Values[13], Values[14], Values[15]
        );
    }

    Mat4x4 Mat4x4::Zero()
    {
        return Mat4x4(
            { 
                0.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f 
            });
    }

    Mat4x4 Mat4x4::Identity()
    {
        return GlmMat4ToMatrix(glm::mat4(1.0f));
    }

    Mat4x4 Mat4x4::FromPosition(const Vector3& position)
    {
        const auto& glmMat = glm::translate(glm::mat4(1.0f), glm::vec3(position.X, position.Y, position.Z));
        return GlmMat4ToMatrix(glmMat);
    }

    Mat4x4 Mat4x4::FromRotation(const Quaternion& rotation)
    {
        const auto& glmQuat = glm::quat(rotation.W, rotation.X, rotation.Y, rotation.Z);
        const auto& glmMat = glm::mat4_cast(glm::normalize(glmQuat));
        return GlmMat4ToMatrix(glmMat);
    }

    Mat4x4 Mat4x4::FromScale(const Vector3& scale)
    {
        const auto& glmMat = glm::scale(glm::mat4(1.0f), glm::vec3(scale.X, scale.Y, scale.Z));
        return GlmMat4ToMatrix(glmMat);
    }

    Mat4x4 Mat4x4::FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        const auto& positionMatrix = FromPosition(position);
        const auto& scaleMatrix = FromScale(scale);
        const auto& rotationMatrix = FromRotation(rotation);
        const auto& result = positionMatrix * rotationMatrix * scaleMatrix;
        return result;
    }

    Mat4x4 Mat4x4::LookAt(Vector3 from, Vector3 target, Vector3 up)
    {
        const auto& glmVecFrom = glm::vec3(from.X, from.Y, from.Z);
        const auto& glmVecTarget = glm::vec3(target.X, target.Y, target.Z);
        const auto& glmVecUp = glm::vec3(up.X, up.Y, up.Z);

        return GlmMat4ToMatrix(glm::lookAt(glmVecFrom, glmVecTarget, glmVecUp));
    }

    Mat4x4 Mat4x4::OrthographicProjection(const Bounds& bounds, float zNear, float zFar)
    {
        const auto& glmMat = glm::ortho(bounds.Left, bounds.Right, bounds.Bottom, bounds.Top, zNear, zFar);
        return GlmMat4ToMatrix(glmMat);
    }

    Mat4x4 Mat4x4::PerspectiveProjection(float fov, float aspect, float zNear, float zFar)
    {
        const auto& glmMat = glm::perspective(fov, aspect, zNear, zFar);
        return GlmMat4ToMatrix(glmMat);
    }

    Mat4x4 Mat4x4::Inverse(const Mat4x4& matrix)
    {
        const auto& glmMat = glm::make_mat4(matrix.Values.data());
        const auto& inversedGlmMat = glm::inverse(glmMat);
        return GlmMat4ToMatrix(inversedGlmMat);
    }

    Mat4x4 Mat4x4::Translate(const Mat4x4& matrix, const Vector3& translate)
    {
        auto result = matrix;

        result[3]  *= translate.X; // move along the x-axis
        result[7]  *= translate.Y; // move along the y-axis
        result[11] *= translate.Z; // move along the z-axis

        return result;
    }

    Mat4x4 Mat4x4::Rotate(const Mat4x4& matrix, const Quaternion& rotation)
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

    Mat4x4 Mat4x4::Scale(const Mat4x4& matrix, const Vector3& scale)
    {
        auto result = matrix;

        result[0]  *= scale.X; // scale along the x-axis
        result[5]  *= scale.Y; // scale along the y-axis
        result[10] *= scale.Z; // scale along the z-axis

        return result;
    }

    Mat4x4 Mat4x4::Add(const Mat4x4& lhs, const Mat4x4& rhs)
    {
        const auto& lhsMat = glm::make_mat4(lhs.Values.data());
        const auto& rhsMat = glm::make_mat4(rhs.Values.data());

        const auto& result = lhsMat + rhsMat;
        return GlmMat4ToMatrix(result);
    }

    Mat4x4 Mat4x4::Subtract(const Mat4x4& lhs, const Mat4x4& rhs)
    {
        const auto& lhsMat = glm::make_mat4(lhs.Values.data());
        const auto& rhsMat = glm::make_mat4(rhs.Values.data());

        const auto& result = lhsMat - rhsMat;
        return GlmMat4ToMatrix(result);
    }

    Mat4x4 Mat4x4::Multiply(const Mat4x4& lhs, const Mat4x4& rhs)
    {
        const auto& lhsMat = glm::make_mat4(lhs.Values.data());
        const auto& rhsMat = glm::make_mat4(rhs.Values.data());

        const auto& result = lhsMat * rhsMat;
        return GlmMat4ToMatrix(result);
    }

    Mat4x4 Mat4x4::Multiply(float lhs, const Mat4x4& rhs)
    {
        const auto& rhsMat = glm::make_mat4(rhs.Values.data());
        const auto& result = lhs * rhsMat;
        return GlmMat4ToMatrix(result);
    }

    Mat4x4 Mat4x4::Multiply(const Mat4x4& lhs, float rhs)
    {
        const auto& lhsMat = glm::make_mat4(lhs.Values.data());
        const auto& result = lhsMat * rhs;
        return GlmMat4ToMatrix(result);
    }
}