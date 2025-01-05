#pragma once
#include "TbxAPI.h"
#include "Math/Bounds.h"
#include "Math/Matrices.h"
#include "Math/Vectors.h"
#include "Math/Quaternion.h"

namespace Tbx
{
    // TODO: Make this camera used  by the renderer and test it!
    // Follow this tutorial: https://www.youtube.com/watch?v=NjKv-HWstxA&list=PLlrATfBNZ98dC-V-N3m0Go4deliWHPFwT&index=36
    class TBX_API Camera
    {
    public:
        Camera();
        Camera(const Bounds& bounds, float zNear, float zFar);
        Camera(float fov, float aspect, float zNear, float zFar);

        void Update();

        void SetOrthagraphic(const Bounds& bounds, float zNear, float zFar);
        void SetPerspective(float fov, float aspect, float zNear, float zFar);

        const Vector3& GetPosition() const { return _position; }
        void SetPosition(const Vector3& position) { _position = position; }

        const Quaternion& GetRotation() const { return _rotation; }
        void SetRotation(const Quaternion& rotation) { _rotation = rotation; }

        const Matrix4x4& GetViewProjectionMatrix() const { return _viewProjectionMatrix; }
        const Matrix4x4& GetProjectionMatrix() const { return _projectionMatrix; }
        const Matrix4x4& GetViewMatrix() const { return _viewMatrix; }

    private:
        Matrix4x4 _viewProjectionMatrix;
        Matrix4x4 _projectionMatrix;
        Matrix4x4 _viewMatrix;

        Vector3 _position;
        Quaternion _rotation;

        void RecalculateMatrices();
    };
}
