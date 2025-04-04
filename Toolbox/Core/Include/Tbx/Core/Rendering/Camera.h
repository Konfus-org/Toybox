#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Bounds.h"
#include "Tbx/Core/Math/Mat4x4.h"
#include "Tbx/Core/Math/Vectors.h"
#include "Tbx/Core/Math/Quaternion.h"
#include "Tbx/Core/Math/Trig.h"

namespace Tbx
{
    // TODO: this should be a component (block) in the future. We need to also implement the idea of game objects (Toys) and components (blocks)
    class EXPORT Camera
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
        const Mat4x4& GetViewProjectionMatrix() const { return _viewProjectionMatrix; }
        const Mat4x4& GetProjectionMatrix() const { return _projectionMatrix; }
        const Mat4x4& GetViewMatrix() const { return _viewMatrix; }

    private:
        bool _isPerspective = true;
        float _zNear = 0.1f;
        float _zFar = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;

        Vector3 _position = Vector3::Zero();
        Quaternion _rotation = Quaternion::Identity();

        Mat4x4 _viewProjectionMatrix = Mat4x4::Identity();
        Mat4x4 _projectionMatrix = Mat4x4::Identity();
        Mat4x4 _viewMatrix = Mat4x4::Identity();

        void RecalculateViewProjection();
    };
}
