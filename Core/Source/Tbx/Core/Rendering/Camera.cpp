#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Rendering/Camera.h"
#include "Tbx/Core/Math/Mat4x4.h"

namespace Tbx
{
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

    Mat4x4 Camera::CalculateViewMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& camProjection)
    {
        const auto& flipXVector = Vector3(-1, 1, 1);
        const auto& cameraViewPos = camPosition * flipXVector;
        const auto& lookAtPos = cameraViewPos + Vector3::Forward();
        const auto& rotationMatrix = Mat4x4::FromRotation(camRotation);

        auto viewMatrix = Mat4x4::LookAt(cameraViewPos, lookAtPos, Vector3::Up()) * rotationMatrix;
        return viewMatrix;
    }

    Mat4x4 Camera::CalculateViewProjectionMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& camProjection)
    {
        auto viewMatrix = CalculateViewMatrix(camPosition, camRotation, camProjection);
        auto viewProjectionMatrix = camProjection * viewMatrix;
        return viewMatrix;
    }
}