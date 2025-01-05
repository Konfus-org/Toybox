#include "TbxPCH.h"
#include "Camera.h"

namespace Tbx
{
    Camera::Camera()
    {
        _position = Vector3::Zero();
        _rotation = Quaternion::Identity();

        SetPerspective(60.0f, 1.0f, 0.1f, 100.0f);
        RecalculateMatrices();
    }

    Camera::Camera(const Bounds& bounds, float zNear, float zFar)
    {
        _position = Vector3::Zero();
        _rotation = Quaternion::Identity();

        SetOrthagraphic(bounds, zNear, zFar);
        RecalculateMatrices();
    }

    Camera::Camera(float fov, float aspect, float zNear, float zFar)
    {
        _position = Vector3::Zero();
        _rotation = Quaternion::Identity();

        SetPerspective(fov, aspect, zNear, zFar);
        RecalculateMatrices();
    }

    void Camera::Update()
    {
        RecalculateMatrices();
    }

    void Camera::SetOrthagraphic(const Bounds& bounds, float zNear, float zFar)
    {
        _projectionMatrix = Matrix4x4::OrthographicProjection(bounds, zNear, zFar);
    }

    void Camera::SetPerspective(float fov, float aspect, float zNear, float zFar)
    {
        _projectionMatrix = Matrix4x4::PerspectiveProjection(fov, aspect, zNear, zFar);
    }

    void Camera::RecalculateMatrices()
    {
        const auto& translationMatrix = Matrix4x4::FromPosition(_position);
        const auto& rotationMatrix = Matrix4x4::FromRotation(_rotation);
        auto transformMatrix = rotationMatrix * translationMatrix;

        _viewMatrix = Matrix4x4::Inverse(transformMatrix.Data);
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }
}