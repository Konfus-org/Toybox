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
        _projectionMatrix = Matrix::PerspectiveProjection(_bounds, zNear, zFar);

        RecalculateMatrices();
    }

    void Camera::SetAspect(float aspect)
    {
        if (_aspect == aspect) return;

        if (_isPerspective)
        {
            SetPerspective(_fov, _aspect, _zNear, _zFar);
        }
        else
        {
            auto orthographicSize = (_bounds.Left + _bounds.Top) / 2.0f;
            SetOrthagraphic(orthographicSize, aspect, _zNear, _zFar);
        }
    }

    void Camera::RecalculateMatrices()
    {
        const auto& translationMatrix = Matrix::FromPosition(_position);
        const auto& rotationMatrix = Matrix::FromRotation(_rotation);
        auto transformMatrix = rotationMatrix * translationMatrix;

        _viewMatrix = Matrix::Inverse(transformMatrix.Values);
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }
}