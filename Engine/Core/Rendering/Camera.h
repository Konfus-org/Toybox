#pragma once
#include "TbxAPI.h"
#include "Math/Bounds.h"
#include "Math/Matrix.h"
#include "Math/Vectors.h"
#include "Math/Quaternion.h"

namespace Tbx
{
    class TBX_API Camera
    {
    public:
        Camera();

        void SetOrthagraphic(float size, float aspect, float zNear, float zFar);
        void SetPerspective(float fov, float aspect, float zNear, float zFar);
        void SetAspect(float aspect);

        const Vector3& GetPosition() const { return _position; }
        void SetPosition(const Vector3& position) { _position = position; RecalculateMatrices(); }

        const Quaternion& GetRotation() const { return _rotation; }
        void SetRotation(const Quaternion& rotation) { _rotation = rotation; RecalculateMatrices(); }

        bool IsPerspective() const { return _isPerspective; }
        bool IsOrthagraphic() const { return !_isPerspective; }

        const Bounds& GetBounds() const { return _bounds; }
        const Matrix& GetViewProjectionMatrix() const { return _viewProjectionMatrix; }
        const Matrix& GetProjectionMatrix() const { return _projectionMatrix; }
        const Matrix& GetViewMatrix() const { return _viewMatrix; }

    private:
        bool _isPerspective = true;
        float _zNear = 0.1f;
        float _zFar = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;

        Bounds _bounds = Bounds::Identity();
        Vector3 _position = Vector3::Zero();
        Quaternion _rotation = Quaternion::Identity();

        Matrix _viewProjectionMatrix = Matrix::Identity();
        Matrix _projectionMatrix = Matrix::Identity();
        Matrix _viewMatrix = Matrix::Identity();

        void RecalculateMatrices();
    };
}
