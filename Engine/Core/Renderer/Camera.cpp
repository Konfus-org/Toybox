#include "TbxPCH.h"
#include "Camera.h"

namespace Tbx
{
    Camera::Camera()
    {
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }

    void Camera::SetOrthagraphic(float size, float aspect, float zNear, float zFar)
    {
        const auto& bounds = Bounds::FromOrthographicProjection(size, aspect);

        _isPerspective = false;
        _zNear = zNear;
        _zFar = zFar;
        _fov = size;
        _aspect = aspect;
        _projectionMatrix = Matrix::OrthographicProjection(bounds, zNear, zFar);

        RecalculateViewProjection();
    }

    void Camera::SetPerspective(float fov, float aspect, float zNear, float zFar)
    {
        const auto& fovRadians = Math::DegreesToRadians(fov);

        _isPerspective = true;
        _zNear = zNear;
        _zFar = zFar;
        _aspect = aspect;
        _fov = fovRadians;
        _projectionMatrix = Matrix::PerspectiveProjection(fovRadians, aspect, zNear, zFar);

        RecalculateViewProjection();
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

    void Camera::RecalculateViewProjection()
    {
        const auto& flipXVector = Vector3(-1, 1, 1);
        const auto& cameraViewPos = _position * flipXVector;
        const auto& lookAtPos = cameraViewPos + Vector3::Forward();
        const auto& rotationMatrix = Matrix::FromRotation(_rotation);

        _viewMatrix = Matrix::LookAt(cameraViewPos, lookAtPos, {0.0f, 1.0f, 0.0f}) * rotationMatrix;
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }
}