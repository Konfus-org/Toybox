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
        _isPerspective = false;
        _zNear = zNear;
        _zFar = zFar;
        _aspect = aspect;
        _bounds = Bounds::FromOrthographicProjection(size, aspect);
        _projectionMatrix = Matrix::OrthographicProjection(_bounds, zNear, zFar);

        RecalculateMatrices();
    }

    void Camera::SetPerspective(float fov, float aspect, float zNear, float zFar)
    {
        _isPerspective = true;
        _zNear = zNear;
        _zFar = zFar;
        _aspect = aspect;
        _bounds = Bounds::FromPerspectiveProjection(fov, aspect, zNear);
        _projectionMatrix = Matrix::PerspectiveProjection(fov, aspect, zNear, zFar);

        RecalculateMatrices();
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
            const auto& size = _bounds.Bottom;
            SetOrthagraphic(size, aspect, _zNear, _zFar);
        }
    }

    void Camera::RecalculateMatrices()
    {
        const auto& translationMatrix = Matrix::FromPosition(_position);
        const auto& rotationMatrix = Matrix::FromRotation(_rotation);
        const auto& transformMatrix = rotationMatrix * translationMatrix;

        _viewMatrix = Matrix::Inverse(transformMatrix);
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }
}