#include "Tbx/PCH.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Trig.h"
#include "Tbx/Graphics/Frustum.h"

namespace Tbx
{
    Camera::Camera()
    {
        // Defaults to perspective, need to calc default projection matrix
        SetPerspective(_fov, _aspect, _zNear, _zFar);
    }

    void Camera::SetOrthagraphic(float size, float aspect, float zNear, float zFar)
    {
        const auto& bounds = Bounds::FromOrthographicProjection(size, aspect);

        _isPerspective = false;
        _zNear = zNear;
        _zFar = zFar;
        _fov = size;
        _aspect = aspect;
        _projectionMatrix = Mat4x4::OrthographicProjection(bounds, zNear, zFar);
    }

    void Camera::SetPerspective(float fov, float aspect, float zNear, float zFar)
    {
        _isPerspective = true;
        _zNear = zNear;
        _zFar = zFar;
        _aspect = aspect;
        _fov = fov;
        _projectionMatrix = Mat4x4::PerspectiveProjection(Math::DegreesToRadians(fov), aspect, zNear, zFar);
    }

    void Camera::SetAspect(float aspect)
    {
        if (_aspect == aspect) return;

        if (_isPerspective)
        {
            SetPerspective(_fov, aspect, _zNear, _zFar);
        }
        else
        {
            SetOrthagraphic(_fov, aspect, _zNear, _zFar);
        }
    }

    Mat4x4 Camera::CalculateViewMatrix(const Vector3& camPosition, const Quaternion& camRotation)
    {
        // 1. Get the inverse of the camera's rotation
        Mat4x4 rotationMatrix = Mat4x4::FromRotation(camRotation);
        Mat4x4 inverseRotationMatrix = Mat4x4::Inverse(rotationMatrix); // Or rotationMatrix.Transpose() for orthonormal matrices

        // 2. Get the inverse of the camera's position
        Mat4x4 translationMatrix = Mat4x4::Translate(Constants::Mat4x4::Identity, camPosition * -1);

        // The view matrix first rotates the world opposite to the camera's rotation,
        // then translates the world opposite to the camera's position.
        return inverseRotationMatrix * translationMatrix;
    }

    Mat4x4 Camera::CalculateViewProjectionMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& projectionMatrix)
    {
        Mat4x4 viewMatrix = CalculateViewMatrix(camPosition, camRotation);
        return projectionMatrix * viewMatrix;
    }

    Frustum Camera::CalculateFrustum(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& projectionMatrix)
    {
        const auto viewProj = CalculateViewProjectionMatrix(camPosition, camRotation, projectionMatrix);
        return Frustum(viewProj);
    }
}