#include "TbxPCH.h"
#include "Camera.h"

namespace Tbx
{
    Camera::Camera()
    {
        SetPerspective(60.0f, 1.0f, 0.1f, 100.0f);

        _position = Vector3::Zero();
        _rotation = Quaternion::Identity();
        _viewMatrix = Matrix::Identity();
        _viewProjectionMatrix = Matrix::Identity();
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }

    Camera::Camera(const Bounds& bounds, float zNear, float zFar)
    {
        SetOrthagraphic(bounds, zNear, zFar);

        _position = Vector3::Zero();
        _rotation = Quaternion::Identity();
        _viewMatrix = Matrix::Identity();
        _viewProjectionMatrix = Matrix::Identity();
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }

    Camera::Camera(float fov, float aspect, float zNear, float zFar)
    {
        SetPerspective(fov, aspect, zNear, zFar);

        _position = Vector3::Zero();
        _rotation = Quaternion::Identity();
        _viewMatrix = Matrix::Identity();
        _viewProjectionMatrix = Matrix::Identity();
        _viewProjectionMatrix = _projectionMatrix * _viewMatrix;
    }

    void Camera::Update()
    {
        //RecalculateMatrices();
    }

    void Camera::SetOrthagraphic(const Bounds& bounds, float zNear, float zFar)
    {
        _projectionMatrix = Matrix::OrthographicProjection(bounds, zNear, zFar);
    }

    void Camera::SetPerspective(float fov, float aspect, float zNear, float zFar)
    {
        _projectionMatrix = Matrix::PerspectiveProjection(fov, aspect, zNear, zFar);
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