#pragma once
#include "TbxAPI.h"
#include "Math/Bounds.h"
#include "Math/Matrix.h"
#include "Math/Vectors.h"
#include "Math/Quaternion.h"
#include "Math/Trig.h"

namespace Tbx
{
    // TODO: this should be a component (block) in the future. We need to also implement the idea of game objects (Toys) and components (blocks)
    class TBX_API Camera
    {
    public:
        Camera();

        void SetOrthagraphic(float size, float aspect, float zNear, float zFar);
        void SetPerspective(float fov, float aspect, float zNear, float zFar);
        void SetAspect(float aspect);

        // TODO: this should all be in a seperate "Transform" component (block) in the future
        const Vector3& GetPosition() const { return _position; }
        void SetPosition(const Vector3& position) { _position = position; RecalculateViewProjection(); }

        const Quaternion& GetRotation() const { return _rotation; }
        void SetRotation(const Quaternion& rotation) { _rotation = rotation; RecalculateViewProjection(); }

        bool IsPerspective() const { return _isPerspective; }
        bool IsOrthagraphic() const { return !_isPerspective; }

        float GetFov() const { return Math::RadiansToDegrees(_fov); }
        const Matrix& GetViewProjectionMatrix() const { return _viewProjectionMatrix; }
        const Matrix& GetProjectionMatrix() const { return _projectionMatrix; }
        const Matrix& GetViewMatrix() const { return _viewMatrix; }

    private:
        bool _isPerspective = true;
        float _zNear = 0.1f;
        float _zFar = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;

        Vector3 _position = Vector3::Zero();
        Quaternion _rotation = Quaternion::Identity();

        Matrix _viewProjectionMatrix = Matrix::Identity();
        Matrix _projectionMatrix = Matrix::Identity();
        Matrix _viewMatrix = Matrix::Identity();

        void RecalculateViewProjection();
    };
}
