#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Rendering/Camera.h"
#include "Tbx/Core/Math/Mat4x4.h"
#include "Tbx/Core/Math/Trig.h"

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
        // Calculate the view matrix
        Mat4x4 rotationMatrix = Mat4x4::FromRotation(camRotation);
        Mat4x4 translationMatrix = Mat4x4::Translate(Mat4x4::Identity(), camPosition * Vector3(-1, 1, 1) * -1);

        return rotationMatrix * translationMatrix;
    }

    Mat4x4 Camera::CalculateViewProjectionMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& projectionMatrix)
    {
        Mat4x4 viewMatrix = CalculateViewMatrix(camPosition, camRotation);
        return projectionMatrix * viewMatrix;
    }

    //void Camera::RecalculateViewProjection()
    //{
    //    const auto& flipXVector = Vector3(-1, 1, 1);
    //    const auto& cameraViewPos = _position * flipXVector;
    //    const auto& lookAtPos = cameraViewPos + Vector3::Forward();
    //    const auto& rotationMatrix = Matrix::FromRotation(_rotation);

    //    _viewMatrix = Matrix::LookAt(cameraViewPos, lookAtPos, Vector3::Up()) * rotationMatrix;
    //    _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    //}
}