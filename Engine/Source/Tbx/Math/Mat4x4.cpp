#include "Tbx/PCH.h"
#include "Tbx/Math/Constants.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Trig.h"
#include "Tbx/TypeAliases/Int.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Tbx
{
    static Mat4x4 GlmMat4ToTbxMat4x4(const glm::mat4& glmMat)
    {
        std::array<float, 16> arr;
        std::memcpy(arr.data(), glm::value_ptr(glmMat), sizeof(arr));
        return arr;
    }

    Mat4x4::Mat4x4()
    {
        Values = Constants::Mat4x4::Identity;
    }

    Mat4x4::Mat4x4(const std::initializer_list<float>& data)
    {
        std::ranges::copy(data, Values.begin());
    }

    Mat4x4::Mat4x4(const std::initializer_list<std::initializer_list<float>>& data)
    {
        // Copy init list to dest val
        uint row = 0;
        std::array<std::array<float, 4>, 4> vals;
        for (const auto& rowList : data)
        {
            uint col = 0;
            for (float val : rowList)
            {
                if (row < 4 && col < 4)
                {
                    vals[row][col] = val;
                    col++;
                }
                else throw std::out_of_range("Initializer list has more elements than the destination array.");
            }

            if (col != 4 && row < 4)  throw std::out_of_range("Initializer list row has fewer elements than the destination array row.");
            row++;
        }
        if (row != 4)  throw std::out_of_range("Initializer list has fewer rows than the destination array.");

        // Copy result to values
        Values =
        {
            vals[0][0], vals[1][0], vals[2][0], vals[3][0],
            vals[0][1], vals[1][1], vals[2][1], vals[3][1],
            vals[0][2], vals[1][2], vals[2][2], vals[3][2],
            vals[0][3], vals[1][3], vals[2][3], vals[3][3]
        };
    }

    Mat4x4::Mat4x4(const std::array<std::array<float, 4>, 4>& data)
    {
        Values = 
        {
            data[0][0], data[1][0], data[2][0], data[3][0],
            data[0][1], data[1][1], data[2][1], data[3][1],
            data[0][2], data[1][2], data[2][2], data[3][2],
            data[0][3], data[1][3], data[2][3], data[3][3]
        };
    }

    std::string Mat4x4::ToString() const
    {
        return std::format(
            "[{}, {}, {}, {}],\n[{}, {}, {}, {}],\n[{}, {}, {}, {}],\n[{}, {}, {}, {}]",
            Values[0], Values[4], Values[8], Values[12],
            Values[1], Values[5], Values[9], Values[13],
            Values[2], Values[6], Values[10], Values[14],
            Values[3], Values[7], Values[11], Values[15]
        );
    }

    Mat4x4 Mat4x4::FromPosition(const Vector3& position)
    {
        const glm::mat4 glmMat = glm::translate(glm::mat4(1.0f), glm::vec3(position.X, position.Y, position.Z));
        return GlmMat4ToTbxMat4x4(glmMat);
    }

    Mat4x4 Mat4x4::FromRotation(const Quaternion& rotation)
    {
        const auto glmQuat = glm::quat(rotation.W, rotation.X, rotation.Y, rotation.Z);
        glm::mat4 rotationMatrix = glm::toMat4(glmQuat);
        return GlmMat4ToTbxMat4x4(rotationMatrix);
    }

    Mat4x4 Mat4x4::FromScale(const Vector3& scale)
    {
        const glm::mat4 glmMat = glm::scale(glm::mat4(1.0f), glm::vec3(scale.X, scale.Y, scale.Z));
        return GlmMat4ToTbxMat4x4(glmMat);
    }

    Mat4x4 Mat4x4::FromTRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        const auto glmPos = glm::vec3(position.X, position.Y, position.Z);
        const auto glmRot = glm::quat(rotation.W, rotation.X, rotation.Y, rotation.Z);
        const auto glmScale = glm::vec3(scale.X, scale.Y, scale.Z);

        glm::mat4 glmMat = glm::translate(glm::mat4(1.0f), glmPos)
            * glm::mat4_cast(glmRot)
            * glm::scale(glm::mat4(1.0f), glmScale);
        return GlmMat4ToTbxMat4x4(glmMat);
    }

    Mat4x4 Mat4x4::LookAt(const Vector3& from, const Vector3& target, const Vector3& up)
    {
        const auto glmVecFrom = glm::vec3(from.X, from.Y, from.Z);
        const auto glmVecTarget = glm::vec3(target.X, target.Y, target.Z);
        const auto glmVecUp = glm::vec3(up.X, up.Y, up.Z);

        return GlmMat4ToTbxMat4x4(glm::lookAt(glmVecFrom, glmVecTarget, glmVecUp));
    }

    Mat4x4 Mat4x4::OrthographicProjection(const Bounds& bounds, float zNear, float zFar)
    {
        const glm::mat4& glmMat = glm::ortho(
            bounds.Left,
            bounds.Right,
            bounds.Bottom,
            bounds.Top,
            0.0f,
            zFar - zNear);
        return GlmMat4ToTbxMat4x4(glmMat);
    }

    Mat4x4 Mat4x4::PerspectiveProjection(float fov, float aspect, float zNear, float zFar)
    {
        const glm::mat4 glmMat = glm::perspective(fov, aspect, zNear, zFar);
        return GlmMat4ToTbxMat4x4(glmMat);
    }

    Mat4x4 Mat4x4::Inverse(const Mat4x4& matrix)
    {
        const glm::mat4 glmMat = glm::make_mat4(matrix.Values.data());
        const glm::mat4 inversedGlmMat = glm::inverse(glmMat);
        return GlmMat4ToTbxMat4x4(inversedGlmMat);
    }

    Mat4x4 Mat4x4::Transpose(const Mat4x4& matrix)
    {
        const glm::mat4 glmMat = glm::make_mat4(matrix.Values.data());
        const glm::mat4 result = glm::transpose(glmMat);
        return GlmMat4ToTbxMat4x4(result);
    }

    Mat4x4 Mat4x4::Translate(const Mat4x4& matrix, const Vector3& translate)
    {
        const glm::mat4 glmMat = glm::make_mat4(matrix.Values.data());
        return GlmMat4ToTbxMat4x4(glm::translate(glmMat, glm::vec3(translate.X, translate.Y, translate.Z)));
    }

    Mat4x4 Mat4x4::Rotate(const Mat4x4& matrix, float angle, const Vector3& axis)
    {
        const auto glmVec = glm::vec3(axis.X, axis.Y, axis.Z);
        const glm::mat4 glmMat = glm::make_mat4(matrix.Values.data());
        const glm::mat4 result = glm::rotate(glmMat, Math::DegreesToRadians(angle), glmVec);
        return GlmMat4ToTbxMat4x4(result);
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
        const glm::mat4 lhsMat = glm::make_mat4(lhs.Values.data());
        const glm::mat4 rhsMat = glm::make_mat4(rhs.Values.data());

        const glm::mat4& result = lhsMat + rhsMat;
        return GlmMat4ToTbxMat4x4(result);
    }

    Mat4x4 Mat4x4::Subtract(const Mat4x4& lhs, const Mat4x4& rhs)
    {
        const glm::mat4 lhsMat = glm::make_mat4(lhs.Values.data());
        const glm::mat4 rhsMat = glm::make_mat4(rhs.Values.data());

        const glm::mat4 result = lhsMat - rhsMat;
        return GlmMat4ToTbxMat4x4(result);
    }

    Mat4x4 Mat4x4::Multiply(const Mat4x4& lhs, const Mat4x4& rhs)
    {
        const glm::mat4 lhsMat = glm::make_mat4(lhs.Values.data());
        const glm::mat4 rhsMat = glm::make_mat4(rhs.Values.data());

        const glm::mat4 result = lhsMat * rhsMat;
        return GlmMat4ToTbxMat4x4(result);
    }

    Mat4x4 Mat4x4::Multiply(float lhs, const Mat4x4& rhs)
    {
        const glm::mat4 rhsMat = glm::make_mat4(rhs.Values.data());
        const glm::mat4 result = lhs * rhsMat;
        return GlmMat4ToTbxMat4x4(result);
    }

    Mat4x4 Mat4x4::Multiply(const Mat4x4& lhs, float rhs)
    {
        const glm::mat4 lhsMat = glm::make_mat4(lhs.Values.data());
        const glm::mat4 result = lhsMat * rhs;
        return GlmMat4ToTbxMat4x4(result);
    }

    bool Mat4x4::IsEqual(const Mat4x4& lhs, float rhs)
    {
        const glm::mat4 lhsMat = glm::make_mat4(lhs.Values.data());
        const glm::mat4 rhsMat = glm::make_mat4(lhs.Values.data());
        return lhsMat == rhsMat;
    }
}
