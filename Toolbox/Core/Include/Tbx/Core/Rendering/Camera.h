#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Bounds.h"
#include "Tbx/Core/Math/Mat4x4.h"
#include "Tbx/Core/Math/Vectors.h"
#include "Tbx/Core/Math/Quaternion.h"
#include "Tbx/Core/Math/Trig.h"

namespace Tbx
{
    class EXPORT Camera
    {
    public:
        Camera();

        void SetOrthagraphic(float size, float aspect, float zNear, float zFar);
        void SetPerspective(float fov, float aspect, float zNear, float zFar);
        void SetAspect(float aspect);

        bool IsPerspective() const { return _isPerspective; }
        bool IsOrthagraphic() const { return !_isPerspective; }

        float GetFov() const { return Math::RadiansToDegrees(_fov); }
        const Mat4x4& GetProjectionMatrix() const { return _projectionMatrix; }

        static Mat4x4 CalculateViewMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& camProjection);
        static Mat4x4 CalculateViewProjectionMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& camProjection);

    private:
        Mat4x4 _projectionMatrix = Mat4x4::Identity();

        bool _isPerspective = true;
        float _zNear = 0.1f;
        float _zFar = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;
    };
}
