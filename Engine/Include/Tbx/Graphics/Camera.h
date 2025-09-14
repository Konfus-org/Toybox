#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Constants.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Vectors.h"
#include "Tbx/Math/Quaternion.h"
#include "Tbx/Graphics/Frustum.h"

namespace Tbx
{
    class Camera
    {
    public:
        EXPORT Camera();

        EXPORT void SetOrthagraphic(float size, float aspect, float zNear, float zFar);
        EXPORT void SetPerspective(float fov, float aspect, float zNear, float zFar);
        EXPORT void SetAspect(float aspect);

        EXPORT bool IsPerspective() const { return _isPerspective; }
        EXPORT bool IsOrthagraphic() const { return !_isPerspective; }

        EXPORT float GetAspect() const { return _aspect; }
        EXPORT float GetFov() const { return _fov; }
        EXPORT float GetZNear() const { return _zNear; }
        EXPORT float GetZFar() const { return _zFar; }
        EXPORT const Mat4x4& GetProjectionMatrix() const { return _projectionMatrix; }

        EXPORT static Frustum CalculateFrustum(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& projectionMatrix);
        EXPORT static Mat4x4 CalculateViewMatrix(const Vector3& camPosition, const Quaternion& camRotation);
        EXPORT static Mat4x4 CalculateViewProjectionMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& projectionMatrix);

    private:
        Mat4x4 _projectionMatrix = Consts::Mat4x4::Identity;

        bool _isPerspective = true;
        float _zNear = 0.1f;
        float _zFar = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;
    };
}
