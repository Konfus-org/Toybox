#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Constants.h"
#include "Tbx/Core/Math/Mat4x4.h"
#include "Tbx/Core/Math/Vectors.h"
#include "Tbx/Core/Math/Quaternion.h"

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

        float GetAspect() const { return _aspect; }
        float GetFov() const { return _fov; }
        float GetZNear() const { return _zNear; }
        float GetZFar() const { return _zFar; }
        const Mat4x4& GetProjectionMatrix() const { return _projectionMatrix; }

        static Mat4x4 CalculateViewMatrix(const Vector3& camPosition, const Quaternion& camRotation);
        static Mat4x4 CalculateViewProjectionMatrix(const Vector3& camPosition, const Quaternion& camRotation, const Mat4x4& projectionMatrix);

    private:
        Mat4x4 _projectionMatrix = Constants::Mat4x4::Identity;

        bool _isPerspective = true;
        float _zNear = 0.1f;
        float _zFar = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;
    };
}
