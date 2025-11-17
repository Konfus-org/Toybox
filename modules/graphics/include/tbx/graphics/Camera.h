#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Quaternion.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    /// <summary>
    /// Maintains projection parameters and helper math routines for 3D camera transforms.
    /// </summary>
    class TBX_EXPORT Camera
    {
      public:
        Camera();

        void SetOrthagraphic(float size, float aspect, float zNear, float zFar);
        void SetPerspective(float fov, float aspect, float zNear, float zFar);
        void SetAspect(float aspect);

        bool IsPerspective() const
        {
            return _isPerspective;
        }
        bool IsOrthagraphic() const
        {
            return !_isPerspective;
        }

        float GetAspect() const
        {
            return _aspect;
        }
        float GetFov() const
        {
            return _fov;
        }
        float GetZNear() const
        {
            return _zNear;
        }
        float GetZFar() const
        {
            return _zFar;
        }
        const Mat4x4& GetProjectionMatrix() const
        {
            return _projectionMatrix;
        }

      private:
        Mat4x4 _projectionMatrix = Mat4x4::Identity;

        bool _isPerspective = true;
        float _zNear = 0.1f;
        float _zFar = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;
    };

    Frustum GetCameraFrustum(
        const Vector3& camPosition,
        const Quaternion& camRotation,
        const Mat4x4& projectionMatrix);

    Mat4x4 GetCameraViewMatrix(const Vector3& camPosition, const Quaternion& camRotation);

    Mat4x4 GetCameraViewProjectionMatrix(
        const Vector3& camPosition,
        const Quaternion& camRotation,
        const Mat4x4& projectionMatrix);
}
